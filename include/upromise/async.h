#ifndef _UPROMISE_ASYNC_HPP_
#define _UPROMISE_ASYNC_HPP_

#include "upromise.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // async
    // Async is not like async/await in javascript.
    // If not in a task, async will not start immediately, but set to the first task queue.
    typedef struct upromise_async_context_t
    {
        upromise_promise_t *promise;
        int co;
    } upromise_async_context_t;

    typedef void *(*upromise_async_fn)(upromise_async_context_t *context, void **error, void *ctx);

    upromise_promise_t *upromise_async(upromise_dispatcher_t *dispatcher, upromise_async_fn fn, void *ctx);

    typedef struct upromise_await_result_t
    {
        void *ret;
        void *error;
    } upromise_await_result_t;

    upromise_await_result_t upromise_await(upromise_async_context_t *context, upromise_promise_t *promise);

    // generator
    // Generator must be used in a task.
    typedef struct upromise_generator_t
    {
        upromise_ref_count_t rc;
        upromise_dispatcher_t *dispatcher;
        int co;
        int done;
        int need_done;
        void *set_data;
        void *data;
        void *error;
    } upromise_generator_t;

    typedef void *(*upromise_generator_fn)(upromise_generator_t *generator, void **error, void *ctx);

    upromise_generator_t *new_upromise_generator(upromise_dispatcher_t *dispatcher, upromise_generator_fn fn, void *ctx);
    void del_upromise_generator(upromise_generator_t *generator);

    typedef struct upromise_generator_result_t
    {
        int done;
        void *data;
        void *error;
    } upromise_generator_result_t;
    upromise_generator_result_t upromise_generator_next(upromise_generator_t *generator, void *value);
    upromise_generator_result_t upromise_generator_return(upromise_generator_t *generator, void *value);
    upromise_generator_result_t upromise_generator_throw(upromise_generator_t *generator, void *value);

    typedef struct upromise_yield_result_t
    {
        int need_done;
        void *data;
    } upromise_yield_result_t;
    upromise_yield_result_t upromise_yield(upromise_generator_t *generator, void *data);

#define YIELD(var, generator, err, value)                               \
    do                                                                  \
    {                                                                   \
        upromise_yield_result_t ret = upromise_yield(generator, value); \
        if (ret.need_done)                                              \
            return ret.data;                                            \
        var = ret.data;                                                 \
    } while (0)

    // async-generator
    // AsyncGenerator must be used in a task.
    typedef struct upromise_agen_t
    {
        upromise_ref_count_t rc;
        upromise_dispatcher_t *dispatcher;
        int co;
        int done;
        // upromise_promise_t *done_promise;
        int need_done;
        int need_throw;
        void *set_data;
        // upromise_promise_t *next_promise;
        upromise_task_queue_t next_queue;
    } upromise_agen_t;

    typedef void *(*upromise_agen_fn)(upromise_agen_t *agen, void **error, void *ctx);

    upromise_agen_t *new_upromise_agen(upromise_dispatcher_t *dispatcher, upromise_agen_fn fn, void *ctx);
    void del_upromise_agen(upromise_agen_t *agen);

    typedef struct upromise_agen_result_t
    {
        int done;
        void *data;
    } upromise_agen_result_t;

    upromise_promise_t *upromise_agen_next(upromise_agen_t *agen, void *value);
    upromise_promise_t *upromise_agen_return(upromise_agen_t *agen, void *value);
    upromise_promise_t *upromise_agen_throw(upromise_agen_t *agen, void *value);

    typedef struct upromise_ayield_result_t
    {
        int need_done;
        int need_throw;
        void *data;
    } upromise_ayield_result_t;
    upromise_ayield_result_t upromise_ayield(upromise_agen_t *agen, upromise_promise_t *data);

#define AYIELD(var, agen, err, value)                                \
    do                                                               \
    {                                                                \
        upromise_ayield_result_t ret = upromise_ayield(agen, value); \
        if (ret.need_throw)                                          \
        {                                                            \
            *err = ret.data;                                         \
            return NULL;                                             \
        }                                                            \
        if (ret.need_done)                                           \
            return ret.data;                                         \
        var = ret.data;                                              \
    } while (0)

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
namespace upromise
{
    class AsyncContext
    {
        upromise_async_context_t *context;

    public:
        AsyncContext(upromise_async_context_t *context) : context(context) {}

        void *await(Promise promise)
        {
            auto result = upromise_await(context, promise.impl());
            if (result.error != nullptr)
                throw Error{result.error};
            return result.ret;
        }

        struct BodyContext
        {
            std::function<void *(AsyncContext)> fn;
        };

        static void *common_body(upromise_async_context_t *context, void **error, void *ctx_raw)
        {
            BodyContext *ctx = (BodyContext *)ctx_raw;
            void *ret = nullptr;
            try
            {
                ret = ctx->fn(AsyncContext(context));
            }
            catch (Error err)
            {
                ret = nullptr;
                *error = err.err;
            }
            delete ctx;
            return ret;
        }
    };

    // template <typename... Args>
    // inline std::function<Promise(Args...)> async(const std::shared_ptr<Dispatcher> &dispatcher, const std::function<void *(AsyncContext, Args...)> &fn)
    // {
    //     return [=](Args... args) -> Promise
    //     {
    //         auto ctx = new AsyncContext::BodyContext{std::bind(fn, std::placeholders::_1, args...)};
    //         auto promise = upromise_async(dispatcher->dispatcher, &AsyncContext::common_body, ctx);
    //         return Promise(dispatcher, promise);
    //     };
    // }

    template <typename F>
    struct async
    {
        async(const std::shared_ptr<Dispatcher> &dispatcher, F fn) : dispatcher(dispatcher), fn(fn) {}

        template <typename... Args>
        Promise operator()(Args... args) const
        {
            auto ctx = new AsyncContext::BodyContext{std::bind(fn, std::placeholders::_1, args...)};
            auto promise = upromise_async(dispatcher->dispatcher, &AsyncContext::common_body, ctx);
            return Promise(dispatcher, promise);
        }

    private:
        std::shared_ptr<Dispatcher> dispatcher;
        F fn;
    };

    class Generator
    {
        std::shared_ptr<Dispatcher> dispatcher;
        upromise_generator_t *generator;

    public:
        using Fn = std::function<void *(Generator *)>;

        Generator() : generator(nullptr) {}

        Generator(const std::shared_ptr<Dispatcher> &dispatcher, upromise_generator_t *ptr)
            : dispatcher(dispatcher), generator(ptr) {}

        struct BodyContext
        {
            Fn fn;
            std::shared_ptr<Dispatcher> dispatcher;
        };

        Generator(const std::shared_ptr<Dispatcher> &dispatcher, Fn fn) : dispatcher(dispatcher)
        {
            BodyContext *ptr = new BodyContext();
            ptr->fn = fn;
            ptr->dispatcher = dispatcher;
            generator = new_upromise_generator(dispatcher->dispatcher, &Generator::common_body, ptr);
        }
        ~Generator()
        {
            if (generator)
                del_upromise_generator(generator);
        }
        Generator(const Generator &p)
        {
            generator = p.generator;
            dispatcher = p.dispatcher;
            generator->rc += 1;
        }
        Generator &operator=(const Generator &p)
        {
            generator = p.generator;
            dispatcher = p.dispatcher;
            generator->rc += 1;
            return *this;
        }
        Generator(Generator &&p)
        {
            generator = p.generator;
            p.generator = nullptr;
            dispatcher = std::move(p.dispatcher);
        }
        Generator &operator=(Generator &&p)
        {
            generator = p.generator;
            p.generator = nullptr;
            dispatcher = std::move(p.dispatcher);
            return *this;
        }

        using Result = upromise_agen_result_t;

        Result next(void *value = nullptr)
        {
            return do_result(upromise_generator_next(generator, value));
        }

        Result Return(void *value = nullptr)
        {
            return do_result(upromise_generator_return(generator, value));
        }

        Result Throw(void *value = nullptr)
        {
            return do_result(upromise_generator_throw(generator, value));
        }

        upromise_yield_result_t yield(void *data)
        {
            return upromise_yield(generator, data);
        }

    private:
        Result do_result(upromise_generator_result_t result)
        {
            if (result.error != nullptr)
                throw Error{result.error};
            Result ret;
            ret.done = result.done;
            ret.data = result.data;
            return ret;
        }

        static void *common_body(upromise_generator_t *generator, void **error, void *ctx_raw)
        {
            BodyContext *ctx = (BodyContext *)ctx_raw;
            Generator gen(ctx->dispatcher, generator);
            void *ret = nullptr;
            try
            {
                ret = ctx->fn(&gen);
            }
            catch (Error err)
            {
                *error = err.err;
                ret = nullptr;
            }
            delete ctx;
            return ret;
        }
    };

    template <typename F>
    struct generator
    {
        generator(const std::shared_ptr<Dispatcher> &dispatcher, F fn) : dispatcher(dispatcher), fn(fn) {}

        template <typename... Args>
        Generator operator()(Args... args) const
        {
            return Generator(dispatcher, std::bind(fn, std::placeholders::_1, args...));
        }

    private:
        std::shared_ptr<Dispatcher> dispatcher;
        F fn;
    };

    class AsyncGenerator
    {
        std::shared_ptr<Dispatcher> dispatcher;
        upromise_agen_t *agen;

    public:
        using Fn = std::function<void *(AsyncGenerator *)>;

        AsyncGenerator() : agen(nullptr) {}

        AsyncGenerator(const std::shared_ptr<Dispatcher> &dispatcher, upromise_agen_t *ptr)
            : dispatcher(dispatcher), agen(ptr) {}

        struct BodyContext
        {
            Fn fn;
            std::shared_ptr<Dispatcher> dispatcher;
        };

        AsyncGenerator(const std::shared_ptr<Dispatcher> &dispatcher, Fn fn) : dispatcher(dispatcher)
        {
            BodyContext *ptr = new BodyContext();
            ptr->fn = fn;
            ptr->dispatcher = dispatcher;
            agen = new_upromise_agen(dispatcher->dispatcher, &AsyncGenerator::common_body, ptr);
        }
        ~AsyncGenerator()
        {
            if (agen)
                del_upromise_agen(agen);
        }
        AsyncGenerator(const AsyncGenerator &p)
        {
            agen = p.agen;
            dispatcher = p.dispatcher;
            agen->rc += 1;
        }
        AsyncGenerator &operator=(const AsyncGenerator &p)
        {
            agen = p.agen;
            dispatcher = p.dispatcher;
            agen->rc += 1;
            return *this;
        }
        AsyncGenerator(AsyncGenerator &&p)
        {
            agen = p.agen;
            p.agen = nullptr;
            dispatcher = std::move(p.dispatcher);
        }
        AsyncGenerator &operator=(AsyncGenerator &&p)
        {
            agen = p.agen;
            p.agen = nullptr;
            dispatcher = std::move(p.dispatcher);
            return *this;
        }

        Promise next(void *data = nullptr)
        {
            return Promise(dispatcher, upromise_agen_next(agen, data));
        }

        Promise Return(void *data = nullptr)
        {
            return Promise(dispatcher, upromise_agen_return(agen, data));
        }

        Promise Throw(void *data = nullptr)
        {
            return Promise(dispatcher, upromise_agen_throw(agen, data));
        }

        upromise_ayield_result_t yield(Promise data)
        {
            return upromise_ayield(agen, data.impl());
        }

    private:
        static void *common_body(upromise_agen_t *agen, void **error, void *ctx_raw)
        {
            BodyContext *ctx = (BodyContext *)ctx_raw;
            AsyncGenerator gen(ctx->dispatcher, agen);
            void *ret = nullptr;
            try
            {
                ret = ctx->fn(&gen);
            }
            catch (Error err)
            {
                *error = err.err;
                ret = nullptr;
            }
            delete ctx;
            return ret;
        }
    };

    template <typename F>
    struct agen
    {
        agen(const std::shared_ptr<Dispatcher> &dispatcher, F fn) : dispatcher(dispatcher), fn(fn) {}

        template <typename... Args>
        AsyncGenerator operator()(Args... args) const
        {
            return AsyncGenerator(dispatcher, std::bind(fn, std::placeholders::_1, args...));
        }

    private:
        std::shared_ptr<Dispatcher> dispatcher;
        F fn;
    };
}

#undef YIELD
#define Yield(var, generator, value)        \
    do                                      \
    {                                       \
        auto ret = generator->yield(value); \
        if (ret.need_done)                  \
            return ret.data;                \
        var = ret.data;                     \
    } while (0)

#undef AYIELD
#define AYield(var, agen, value)             \
    do                                       \
    {                                        \
        auto ret = agen->yield(value);       \
        if (ret.need_throw)                  \
            throw upromise::Error{ret.data}; \
        if (ret.need_done)                   \
            return ret.data;                 \
        var = ret.data;                      \
    } while (0)

#endif

#endif
