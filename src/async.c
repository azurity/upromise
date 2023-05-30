#include "upromise/async.h"
#include <stdlib.h>
#include <stdbool.h>

void init_upromise_task_queue(upromise_task_queue_t *queue);
void clear_upromise_task_queue(upromise_task_queue_t *queue);
void upromise_ref_count_inc(upromise_ref_count_t *rc);
bool upromise_ref_count_dec(upromise_ref_count_t *rc);

void run_immediately(upromise_dispatcher_t *dispatcher, upromise_task_t *task)
{
    int current_co = coroutine_running(dispatcher->sch);
    bool yieldable = false;
    if (coroutine_status(dispatcher->sch, current_co) == COROUTINE_RUNNING)
    {
        upromise_task_t *current_task = malloc(sizeof(upromise_task_t));
        current_task->co = current_co;
        current_task->extra = NULL;
        upromise_task_queue_push_immediately(&dispatcher->queue, current_task);
        yieldable = true;
    }
    upromise_task_queue_push_immediately(&dispatcher->queue, task);
    if (yieldable)
        coroutine_yield(dispatcher->sch);
}

// async
typedef struct async_promise_context
{
    upromise_async_fn fn;
    void *ctx;
    upromise_async_context_t *actx;
} async_promise_context;

void async_task_fn(struct schedule *sch, void *ctx_raw)
{
    async_promise_context *ctx = (async_promise_context *)ctx_raw;
    upromise_async_fn fn = ctx->fn;
    void *fn_ctx = ctx->ctx;
    upromise_async_context_t *actx = ctx->actx;
    free(ctx);
    void *error = NULL;
    void *ret = fn(actx, &error, fn_ctx);
    upromise_promise_t *promise = actx->promise;
    free(actx);
    if (error != NULL)
        reject_upromise_promise(promise, error);
    else
        resolve_upromise_promise(promise, ret);
    del_upromise_promise(promise);
}

void async_promise_fn(upromise_promise_t *promise, void *ctx_raw)
{
    async_promise_context *ctx = (async_promise_context *)ctx_raw;
    ctx->actx = malloc(sizeof(upromise_async_context_t));
    ctx->actx->promise = promise;
    upromise_task_t *task = malloc(sizeof(upromise_task_t));
    task->co = coroutine_new(promise->dispatcher->sch, async_task_fn, ctx);
    ctx->actx->co = task->co;
    task->extra = NULL;
    run_immediately(promise->dispatcher, task);
}

upromise_promise_t *upromise_async(upromise_dispatcher_t *dispatcher, upromise_async_fn fn, void *ctx)
{
    async_promise_context *promise_ctx = malloc(sizeof(async_promise_context));
    promise_ctx->fn = fn;
    promise_ctx->ctx = ctx;
    return new_upromise_promise(dispatcher, async_promise_fn, promise_ctx);
}

typedef struct await_promise_context
{
    upromise_async_context_t *context;
    upromise_await_result_t result;
} await_promise_context;

void *await_then_resolve(void *data, void **error, void *ctx_raw)
{
    await_promise_context *ctx = (await_promise_context *)ctx_raw;
    ctx->result.ret = data;
    ctx->result.error = NULL;
    upromise_task_t *task = malloc(sizeof(upromise_task_t));
    task->co = ctx->context->co;
    task->extra = NULL;
    upromise_task_queue_push_immediately(&ctx->context->promise->dispatcher->queue, task);
    return NULL;
}

void *await_then_reject(void *data, void **error, void *ctx_raw)
{
    await_promise_context *ctx = (await_promise_context *)ctx_raw;
    ctx->result.ret = NULL;
    ctx->result.error = data;
    upromise_task_t *task = malloc(sizeof(upromise_task_t));
    task->co = ctx->context->co;
    task->extra = NULL;
    upromise_task_queue_push_immediately(&ctx->context->promise->dispatcher->queue, task);
    return NULL;
}

upromise_await_result_t upromise_await(upromise_async_context_t *context, upromise_promise_t *promise)
{
    await_promise_context *ctx = malloc(sizeof(await_promise_context));
    ctx->context = context;
    upromise_promise_t *temp = upromise_promise_then(promise, ctx, await_then_resolve, await_then_reject);
    del_upromise_promise(temp);
    coroutine_yield(context->promise->dispatcher->sch);
    upromise_await_result_t ret = ctx->result;
    free(ctx);
    return ret;
}

// generator
typedef struct generator_context
{
    upromise_generator_t *generator;
    upromise_generator_fn fn;
    void *ctx;
} generator_context;

void generator_task_fn(struct schedule *sch, void *ctx_raw)
{
    generator_context *ctx = (generator_context *)ctx_raw;
    upromise_generator_t *generator = ctx->generator;
    upromise_generator_fn fn = ctx->fn;
    void *fn_ctx = ctx->ctx;
    free(ctx);
    void *error = NULL;
    upromise_ref_count_inc(&generator->rc);
    void *ret = fn(generator, &error, fn_ctx);
    generator->done = 1;
    generator->error = error;
    if (error == NULL)
        generator->data = ret;
    del_upromise_generator(generator);
    return;
}

upromise_generator_t *new_upromise_generator(upromise_dispatcher_t *dispatcher, upromise_generator_fn fn, void *ctx)
{
    upromise_generator_t *ret = malloc(sizeof(upromise_generator_t));
    ret->rc = 0;
    ret->dispatcher = dispatcher;
    ret->done = 0;
    ret->need_done = 0;
    ret->data = NULL;
    ret->error = NULL;
    ret->set_data = NULL;
    upromise_ref_count_inc(&ret->rc); // for return hold
    upromise_ref_count_inc(&ret->rc); // for fn hold
    generator_context *task_ctx = malloc(sizeof(generator_context));
    task_ctx->generator = ret;
    task_ctx->fn = fn;
    task_ctx->ctx = ctx;
    ret->co = coroutine_new(dispatcher->sch, generator_task_fn, task_ctx);
    return ret;
}

void del_upromise_generator(upromise_generator_t *generator)
{
    if (!upromise_ref_count_dec(&generator->rc))
        return;
    free(generator);
}

upromise_generator_result_t upromise_generator_next(upromise_generator_t *generator, void *value)
{
    upromise_generator_result_t ret;
    if (generator->done)
    {
        ret.done = 1;
        ret.data = generator->data;
        ret.error = generator->error;
        return ret;
    }
    upromise_task_t *task = malloc(sizeof(upromise_task_t));
    task->co = generator->co;
    task->extra = NULL;
    run_immediately(generator->dispatcher, task);
    ret.done = generator->done;
    ret.data = generator->data;
    ret.error = generator->error;
    generator->set_data = value;
    generator->data = NULL;
    generator->error = NULL;
    return ret;
}

upromise_generator_result_t upromise_generator_return(upromise_generator_t *generator, void *value)
{
    generator->need_done = 1;
    upromise_generator_result_t ret = upromise_generator_next(generator, NULL);
    ret.data = value;
    return ret;
}

upromise_generator_result_t upromise_generator_throw(upromise_generator_t *generator, void *value)
{
    generator->need_done = 1;
    upromise_generator_result_t ret = upromise_generator_next(generator, NULL);
    ret.error = value;
    return ret;
}

upromise_yield_result_t upromise_yield(upromise_generator_t *generator, void *data)
{
    generator->data = data;
    coroutine_yield(generator->dispatcher->sch);
    upromise_yield_result_t ret;
    ret.need_done = generator->need_done;
    generator->need_done = 0;
    ret.data = generator->set_data;
    generator->set_data = NULL;
    return ret;
}

// async-generator
typedef struct agen_context
{
    upromise_agen_t *agen;
    upromise_agen_fn fn;
    void *ctx;
} agen_context;

void agen_promise_fn(upromise_promise_t *promise, void *ctx)
{
    del_upromise_promise(promise);
}

void agen_task_fn(struct schedule *sch, void *ctx_raw)
{
    agen_context *ctx = (agen_context *)ctx_raw;
    upromise_agen_t *agen = ctx->agen;
    upromise_agen_fn fn = ctx->fn;
    void *fn_ctx = ctx->ctx;
    free(ctx);
    void *error = NULL;
    void *ret = fn(agen, &error, fn_ctx);
    agen->done = 1;
    while (1)
    {
        upromise_task_t *task = upromise_task_queue_pop(&agen->next_queue);
        if (task == NULL)
            break;
        upromise_promise_t *next_promise = (upromise_promise_t *)task->extra;
        free(task);
        if (error != NULL)
            reject_upromise_promise(next_promise, error);
        else
        {
            upromise_agen_result_t *result = malloc(sizeof(upromise_agen_result_t));
            result->done = 1;
            result->data = ret;
            resolve_upromise_promise(next_promise, result);
        }
        error = NULL;
        ret = NULL;
    }
    del_upromise_agen(agen);
    return;
}

upromise_agen_t *new_upromise_agen(upromise_dispatcher_t *dispatcher, upromise_agen_fn fn, void *ctx)
{
    upromise_agen_t *ret = malloc(sizeof(upromise_agen_t));
    ret->rc = 0;
    ret->dispatcher = dispatcher;
    ret->done = 0;
    ret->need_done = 0;
    ret->need_throw = 0;
    ret->set_data = NULL;
    init_upromise_task_queue(&ret->next_queue);
    upromise_ref_count_inc(&ret->rc); // for return hold
    upromise_ref_count_inc(&ret->rc); // for fn hold
    agen_context *task_ctx = malloc(sizeof(agen_context));
    task_ctx->agen = ret;
    task_ctx->fn = fn;
    task_ctx->ctx = ctx;
    ret->co = coroutine_new(dispatcher->sch, agen_task_fn, task_ctx);
    return ret;
}

void del_upromise_agen(upromise_agen_t *agen)
{
    if (!upromise_ref_count_dec(&agen->rc))
        return;
    clear_upromise_task_queue(&agen->next_queue);
    free(agen);
}

typedef struct agen_next_then_context
{
    upromise_agen_t *agen;
    upromise_promise_t *prev;
    void *over_value;
    int need_done;
    int need_throw;
} agen_next_then_context;

void agen_schedule(agen_next_then_context *ctx)
{
    upromise_agen_t *agen = ctx->agen;
    agen->need_done = ctx->need_done;
    agen->need_throw = ctx->need_throw;
    if (agen->need_done || agen->need_throw)
        agen->set_data = ctx->over_value;
    free(ctx);
    upromise_task_t *task = malloc(sizeof(upromise_task_t));
    task->co = agen->co;
    task->extra = NULL;
    upromise_task_queue_push_immediately(&agen->dispatcher->queue, task);
}

void *agen_next_wait_prev(void *data, void **error, void *ctx_raw)
{
    agen_next_then_context *ctx = (agen_next_then_context *)ctx_raw;
    if (ctx->agen->done)
        free(ctx);
    else
        agen_schedule(ctx);
    return NULL;
}

void agen_next_promise_fn(upromise_promise_t *promise, void *ctx_raw)
{
    agen_next_then_context *ctx = (agen_next_then_context *)ctx_raw;
    if (ctx->prev != NULL)
    {
        upromise_promise_t *temp = upromise_promise_then(ctx->prev, ctx, agen_next_wait_prev, agen_next_wait_prev);
        del_upromise_promise(temp);
        del_upromise_promise(ctx->prev);
    }
    else
        agen_schedule(ctx);
}

upromise_promise_t *upromise_agen_next_impl(upromise_agen_t *agen, void *value, void *over_value, int need_done, int need_throw)
{
    if (agen->done)
    {
        upromise_agen_result_t *result = malloc(sizeof(upromise_agen_result_t));
        result->done = 1;
        result->data = NULL;
        upromise_promise_t *ret = new_upromise_promise(agen->dispatcher, agen_promise_fn, NULL);
        resolve_upromise_promise(ret, result);
        return ret;
    }
    agen_next_then_context *ctx = malloc(sizeof(agen_next_then_context));
    ctx->agen = agen;
    ctx->over_value = over_value;
    ctx->need_done = need_done;
    ctx->need_throw = need_throw;
    ctx->prev = NULL;
    if (agen->next_queue.head != agen->next_queue.tail)
    {
        ctx->prev = (upromise_promise_t *)agen->next_queue.tail->extra;
        upromise_ref_count_inc(&ctx->prev->rc);
    }
    upromise_promise_t *next_promise = new_upromise_promise(agen->dispatcher, agen_next_promise_fn, ctx);
    upromise_task_t *next_task = malloc(sizeof(upromise_task_t));
    next_task->co = (intptr_t)value;
    next_task->extra = next_promise;
    upromise_task_queue_push(&agen->next_queue, next_task);
    return next_promise;
}

upromise_promise_t *upromise_agen_next(upromise_agen_t *agen, void *value)
{
    upromise_agen_next_impl(agen, value, NULL, 0, 0);
}

upromise_promise_t *upromise_agen_return(upromise_agen_t *agen, void *value)
{
    upromise_agen_next_impl(agen, NULL, value, 1, 0);
}

upromise_promise_t *upromise_agen_throw(upromise_agen_t *agen, void *value)
{
    upromise_agen_next_impl(agen, NULL, value, 0, 1);
}

void *ayield_then_resolve(void *data, void **error, void *ctx)
{
    upromise_agen_t *agen = (upromise_agen_t *)ctx;
    upromise_agen_result_t *ret = malloc(sizeof(upromise_agen_result_t));
    ret->done = 0;
    ret->data = data;
    upromise_task_t *task = upromise_task_queue_pop(&agen->next_queue);
    upromise_promise_t *next_promise = (upromise_promise_t *)task->extra;
    agen->set_data = (void *)task->co;
    resolve_upromise_promise(next_promise, ret);
    return NULL;
}

void *ayield_then_reject(void *data, void **error, void *ctx)
{
    upromise_agen_t *agen = (upromise_agen_t *)ctx;
    upromise_task_t *task = upromise_task_queue_pop(&agen->next_queue);
    upromise_promise_t *next_promise = (upromise_promise_t *)task->extra;
    free(task);
    agen->set_data = NULL;
    agen->need_done = true;
    reject_upromise_promise(next_promise, data);

    upromise_task_t *fin_task = malloc(sizeof(upromise_task_t));
    fin_task->co = agen->co;
    fin_task->extra = NULL;
    upromise_task_queue_push_immediately(&agen->dispatcher->queue, fin_task);

    return NULL;
}

upromise_ayield_result_t upromise_ayield(upromise_agen_t *agen, upromise_promise_t *data)
{
    upromise_promise_t *temp = upromise_promise_then(data, agen, ayield_then_resolve, ayield_then_reject);
    del_upromise_promise(temp);
    coroutine_yield(agen->dispatcher->sch);
    upromise_ayield_result_t ret;
    ret.need_done = agen->need_done;
    agen->need_done = 0;
    ret.need_throw = agen->need_throw;
    agen->need_throw = 0;
    ret.data = agen->set_data;
    agen->set_data = NULL;
    return ret;
}
