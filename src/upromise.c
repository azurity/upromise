#include "upromise/upromise.h"
#include <stdlib.h>
#include <stdbool.h>

// ref count
void upromise_ref_count_inc(upromise_ref_count_t *rc)
{
    *rc += 1;
}

bool upromise_ref_count_dec(upromise_ref_count_t *rc)
{
    *rc -= 1;
    return (rc == 0);
}

// task queue
void init_upromise_task_queue(upromise_task_queue_t *queue)
{
    queue->head = malloc(sizeof(upromise_task_t));
    queue->tail = queue->head;
    queue->head->next = NULL;
}

void clear_upromise_task_queue(upromise_task_queue_t *queue)
{
    while (queue->head != NULL)
    {
        upromise_task_t *next = queue->head->next;
        free(queue->head);
        queue->head = next;
    }
}

void upromise_task_queue_push(upromise_task_queue_t *queue, upromise_task_t *task)
{
    task->next = NULL;
    queue->tail->next = task;
    queue->tail = task;
}

void upromise_task_queue_push_immediately(upromise_task_queue_t *queue, upromise_task_t *task)
{
    task->next = queue->head->next;
    if (task->next == NULL)
        queue->tail = task;
    queue->head->next = task;
}

upromise_task_t *upromise_task_queue_pop(upromise_task_queue_t *queue)
{
    if (queue->head == queue->tail)
        return NULL;
    upromise_task_t *ret = queue->head->next;
    queue->head->next = ret->next;
    if (ret->next == NULL)
    {
        queue->tail = queue->head;
    }
    return ret;
}

// dispatcher
upromise_dispatcher_t *new_upromise_dispatcher()
{
    upromise_dispatcher_t *ret = malloc(sizeof(upromise_dispatcher_t));
    ret->sch = coroutine_open();
    init_upromise_task_queue(&ret->queue);
    return ret;
}

void del_upromise_dispatcher(upromise_dispatcher_t *dispatcher)
{
    coroutine_close(dispatcher->sch);
    clear_upromise_task_queue(&dispatcher->queue);
    free(dispatcher);
}

void upromise_dispatcher_run(upromise_dispatcher_t *dispatcher)
{
    while (true)
    {
        upromise_task_t *task = upromise_task_queue_pop(&dispatcher->queue);
        if (task == NULL)
            break;
        coroutine_resume(dispatcher->sch, task->co);
        free(task);
    }
}

// promise
void *upromise_recurse_error = "[promise error] forbid recursively resolving itself";

upromise_promise_t *new_upromise_promise(upromise_dispatcher_t *dispatcher, upromise_promise_fn fn, void *ctx)
{
    upromise_promise_t *ret = malloc(sizeof(upromise_promise_t));
    ret->rc = 0;
    ret->dispatcher = dispatcher;
    ret->state = UPROMISE_PROMISE_STATE_PENDING;
    ret->data = NULL;
    init_upromise_task_queue(&ret->queue);
    upromise_ref_count_inc(&ret->rc); // for return hold
    upromise_ref_count_inc(&ret->rc); // for fn hold
    fn(ret, ctx);
    return ret;
}

void del_upromise_promise(upromise_promise_t *promise)
{
    if (!upromise_ref_count_dec(&promise->rc))
        return;
    clear_upromise_task_queue(&promise->queue);
    free(promise);
}

void resolve_upromise_promise(upromise_promise_t *promise, void *value)
{
    if (promise->state != UPROMISE_PROMISE_STATE_PENDING)
        return;
    promise->data = value;
    promise->state = UPROMISE_PROMISE_STATE_FULFILLED;
    upromise_task_t *cur = upromise_task_queue_pop(&promise->queue);
    while (cur != NULL)
    {
        upromise_task_queue_push(&promise->dispatcher->queue, cur);
        cur = upromise_task_queue_pop(&promise->queue);
    }
}

void reject_upromise_promise(upromise_promise_t *promise, void *reason)
{
    if (promise->state != UPROMISE_PROMISE_STATE_PENDING)
        return;
    promise->data = reason;
    promise->state = UPROMISE_PROMISE_STATE_REJECTED;
    upromise_task_t *cur = upromise_task_queue_pop(&promise->queue);
    while (cur != NULL)
    {
        upromise_task_queue_push(&promise->dispatcher->queue, cur);
        cur = upromise_task_queue_pop(&promise->queue);
    }
}

typedef struct then_context
{
    upromise_promise_t *wait_promise;
    upromise_promise_t *next_promise;
    void *onFulfilled;
    void *onRejected;
    void *ctx;
    bool fulfilled_thenable;
    bool rejected_thenable;
} then_context;

#define UPROMISE_PROMISE_STATE_REDIRECT 1

void resolve_upromise_promise_thenable(upromise_promise_t *promise, upromise_promise_t *value)
{
    if (promise->state != UPROMISE_PROMISE_STATE_PENDING)
        return;
    if (promise == value)
    {
        reject_upromise_promise(promise, upromise_recurse_error);
        return;
    }
    upromise_promise_t *aim = value;
    if (aim->state == UPROMISE_PROMISE_STATE_REDIRECT)
    {
        aim = (upromise_promise_t *)aim->data;
    }
    upromise_task_queue_t *queue;
    if (aim->state == UPROMISE_PROMISE_STATE_PENDING)
    {
        promise->state = UPROMISE_PROMISE_STATE_REDIRECT;
        promise->data = aim;
        queue = &aim->queue;
    }
    else
    {
        promise->state = aim->state;
        promise->data = aim->data;
        aim = promise;
        queue = &promise->dispatcher->queue;
    }
    upromise_task_t *cur = upromise_task_queue_pop(&promise->queue);
    while (cur != NULL)
    {
        ((then_context *)cur->extra)->wait_promise = aim;
        upromise_task_queue_push(queue, cur);
        cur = upromise_task_queue_pop(&promise->queue);
    }
    del_upromise_promise(value);
}

void then_task_fn(struct schedule *sch, void *ctx_raw)
{
    then_context *ctx = (then_context *)ctx_raw;
    void *ret = NULL;
    void *error = NULL;

    void *origin_data = ctx->wait_promise->data;
    upromise_promise_state origin_state = ctx->wait_promise->state;
    void *callback_ctx = ctx->ctx;
    del_upromise_promise(ctx->wait_promise);
    upromise_promise_t *next_promise = ctx->next_promise;
    free(ctx);

    if (origin_state == UPROMISE_PROMISE_STATE_FULFILLED)
    {
        if (ctx->onFulfilled != NULL)
        {
            if (ctx->fulfilled_thenable)
            {
                upromise_promise_t *next = ((upromise_promise_then_fn_thenable)ctx->onFulfilled)(origin_data, &error, callback_ctx);
                if (error == NULL)
                {
                    resolve_upromise_promise_thenable(next_promise, next);
                    del_upromise_promise(next_promise);
                    return;
                }
            }
            else
                ret = ((upromise_promise_then_fn)ctx->onFulfilled)(origin_data, &error, callback_ctx);
        }
        else
            ret = origin_data;
    }
    else if (origin_state == UPROMISE_PROMISE_STATE_REJECTED)
    {
        if (ctx->onRejected != NULL)
        {
            if (ctx->rejected_thenable)
            {
                upromise_promise_t *next = ((upromise_promise_then_fn_thenable)ctx->onRejected)(origin_data, &error, callback_ctx);
                if (error == NULL)
                {
                    resolve_upromise_promise_thenable(next_promise, next);
                    del_upromise_promise(next_promise);
                    return;
                }
            }
            else
                ret = ((upromise_promise_then_fn)ctx->onRejected)(origin_data, &error, callback_ctx);
        }
        else
            error = origin_data;
    }
    else
    {
        int debug = 1;
    }
    if (error != NULL)
        reject_upromise_promise(next_promise, error);
    else
        resolve_upromise_promise(next_promise, ret);
    del_upromise_promise(next_promise);
}

upromise_promise_t *upromise_promise_then_impl(upromise_promise_t *promise, void *ctx, void *onFulfilled, void *onRejected, bool fulfilled_thenable, bool rejected_thenable)
{
    if (promise->state == UPROMISE_PROMISE_STATE_REDIRECT)
        promise = (upromise_promise_t *)promise->data;
    upromise_promise_t *ret = malloc(sizeof(upromise_promise_t));
    ret->rc = 0;
    ret->dispatcher = promise->dispatcher;
    ret->state = UPROMISE_PROMISE_STATE_PENDING;
    ret->data = NULL;
    init_upromise_task_queue(&ret->queue);
    upromise_ref_count_inc(&ret->rc);

    then_context *then_ctx = malloc(sizeof(then_context));
    then_ctx->wait_promise = promise;
    then_ctx->next_promise = ret;
    then_ctx->onFulfilled = onFulfilled;
    then_ctx->onRejected = onRejected;
    then_ctx->ctx = ctx;
    then_ctx->fulfilled_thenable = fulfilled_thenable;
    then_ctx->rejected_thenable = rejected_thenable;
    upromise_ref_count_inc(&promise->rc);
    upromise_ref_count_inc(&ret->rc);

    upromise_task_t *task = malloc(sizeof(upromise_task_t));
    task->co = coroutine_new(promise->dispatcher->sch, then_task_fn, then_ctx);
    task->extra = then_ctx;

    switch (promise->state)
    {
    case UPROMISE_PROMISE_STATE_PENDING:
        upromise_task_queue_push(&promise->queue, task);
        break;
    case UPROMISE_PROMISE_STATE_FULFILLED:
    case UPROMISE_PROMISE_STATE_REJECTED:
        upromise_task_queue_push(&promise->dispatcher->queue, task);
        break;
    }

    return ret;
}

upromise_promise_t *upromise_promise_then(upromise_promise_t *promise, void *ctx, upromise_promise_then_fn onFulfilled, upromise_promise_then_fn onRejected)
{
    return upromise_promise_then_impl(promise, ctx, onFulfilled, onRejected, false, false);
}

upromise_promise_t *upromise_promise_then_thenable_common(upromise_promise_t *promise, void *ctx, upromise_promise_then_fn_thenable onFulfilled, upromise_promise_then_fn onRejected)
{
    return upromise_promise_then_impl(promise, ctx, onFulfilled, onRejected, true, false);
}

upromise_promise_t *upromise_promise_then_common_thenable(upromise_promise_t *promise, void *ctx, upromise_promise_then_fn onFulfilled, upromise_promise_then_fn_thenable onRejected)
{
    upromise_promise_then_impl(promise, ctx, onFulfilled, onRejected, false, true);
}

upromise_promise_t *upromise_promise_then_thenable(upromise_promise_t *promise, void *ctx, upromise_promise_then_fn_thenable onFulfilled, upromise_promise_then_fn_thenable onRejected)
{
    upromise_promise_then_impl(promise, ctx, onFulfilled, onRejected, true, true);
}
