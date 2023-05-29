#ifndef _UPROMISE_H_
#define _UPROMISE_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include "coroutine.h"

    // refcount
    typedef uint32_t upromise_ref_count_t;

    // task queue
    typedef struct upromise_task_t
    {
        struct upromise_task_t *next;
        int co;
        void *extra;
    } upromise_task_t;

    typedef struct upromise_task_queue_t
    {
        upromise_task_t *head;
        upromise_task_t *tail;
    } upromise_task_queue_t;

    void upromise_task_queue_push(upromise_task_queue_t *queue, upromise_task_t *task);
    upromise_task_t *upromise_task_queue_pop(upromise_task_queue_t *queue);

    // dispatcher
    typedef struct upromise_dispatcher_t
    {
        struct schedule *sch;
        upromise_task_queue_t queue;
    } upromise_dispatcher_t;

    upromise_dispatcher_t *new_upromise_dispatcher();
    void del_upromise_dispatcher(upromise_dispatcher_t *dispatcher);
    void upromise_dispatcher_run(upromise_dispatcher_t *dispatcher);

    // promise
    typedef enum upromise_promise_state
    {
        UPROMISE_PROMISE_STATE_PENDING = 0,
        UPROMISE_PROMISE_STATE_FULFILLED = 2,
        UPROMISE_PROMISE_STATE_REJECTED = 3,
    } upromise_promise_state;

    typedef struct upromise_promise_t
    {
        upromise_ref_count_t rc;
        upromise_dispatcher_t *dispatcher;
        upromise_promise_state state;
        void *data;
        upromise_task_queue_t queue;
    } upromise_promise_t;

    typedef void (*upromise_promise_fn)(upromise_promise_t *promise, void *ctx);
    typedef void *(*upromise_promise_then_fn)(void *data, void **error, void *ctx);
    typedef upromise_promise_t *(*upromise_promise_then_fn_thenable)(void *data, void **error, void *ctx);

    upromise_promise_t *new_upromise_promise(upromise_dispatcher_t *dispatcher, upromise_promise_fn fn, void *ctx);
    void del_upromise_promise(upromise_promise_t *promise);
    void resolve_upromise_promise(upromise_promise_t *promise, void *value);
    void reject_upromise_promise(upromise_promise_t *promise, void *reason);
    void resolve_upromise_promise_thenable(upromise_promise_t *promise, upromise_promise_t *value);
    upromise_promise_t *upromise_promise_then(upromise_promise_t *promise, void *ctx, upromise_promise_then_fn onFulfilled, upromise_promise_then_fn onRejected);
    upromise_promise_t *upromise_promise_then_thenable_common(upromise_promise_t *promise, void *ctx, upromise_promise_then_fn_thenable onFulfilled, upromise_promise_then_fn onRejected);
    upromise_promise_t *upromise_promise_then_common_thenable(upromise_promise_t *promise, void *ctx, upromise_promise_then_fn onFulfilled, upromise_promise_then_fn_thenable onRejected);
    upromise_promise_t *upromise_promise_then_thenable(upromise_promise_t *promise, void *ctx, upromise_promise_then_fn_thenable onFulfilled, upromise_promise_then_fn_thenable onRejected);
    extern void *upromise_recurse_error;

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <functional>
#include <memory>
#include <variant>
#include <stdexcept>

namespace upromise
{
    class Dispatcher
    {
        upromise_dispatcher_t *dispatcher;
        friend class Promise;

    public:
        Dispatcher() : dispatcher(nullptr) { dispatcher = new_upromise_dispatcher(); }
        ~Dispatcher()
        {
            if (dispatcher)
                del_upromise_dispatcher(dispatcher);
        }
        Dispatcher(const Dispatcher &) = delete;
        Dispatcher &operator=(const Dispatcher &) = delete;
        Dispatcher(Dispatcher &&d)
        {
            dispatcher = d.dispatcher;
            d.dispatcher = nullptr;
        }
        Dispatcher &operator=(Dispatcher &&d)
        {
            if (dispatcher)
                del_upromise_dispatcher(dispatcher);
            dispatcher = d.dispatcher;
            d.dispatcher = nullptr;
            return *this;
        }
        void run() { upromise_dispatcher_run(dispatcher); }
    };

    class Promise;
    class Thenable
    {
    protected:
        Thenable() {}

    public:
        using Ptr = std::shared_ptr<Thenable>;
        using ResolveNotifyFn = std::function<void(std::variant<void *, Promise, Ptr>)>;
        using NotifyFn = std::function<void(void *)>;
        using Fn = std::function<void(ResolveNotifyFn, NotifyFn)>;

        Fn fn;
        Thenable(Fn fn) : fn(fn) {}
        virtual void then(ResolveNotifyFn resolve, NotifyFn reject)
        {
            return fn(resolve, reject);
        }
    };

    class Promise
    {
        std::shared_ptr<Dispatcher> dispatcher;
        upromise_promise_t *promise;

    public:
        using Resolvable = std::variant<void *, Promise, Thenable::Ptr>;
        using ResolveNotifyFn = std::function<void(Resolvable)>;
        using NotifyFn = std::function<void(void *)>;
        using Fn = std::function<void(ResolveNotifyFn, NotifyFn)>;
        using CallbackFn = std::function<void *(void *)>;
        using ThenableCallbackFn = std::function<Promise(void *)>;
        static inline CallbackFn null = nullptr;

        struct Error
        {
            void *err;
        };

        Promise() : promise(nullptr) {}

        Promise(const std::shared_ptr<Dispatcher> &dispatcher, upromise_promise_t *ptr)
            : dispatcher(dispatcher), promise(ptr)
        {
            promise->rc += 1;
        }

        struct BodyContext
        {
            Fn fn;
            std::shared_ptr<Dispatcher> dispatcher;
        };

        Promise(const std::shared_ptr<Dispatcher> &dispatcher, Fn fn) : dispatcher(dispatcher)
        {
            BodyContext *ptr = new BodyContext();
            ptr->fn = fn;
            ptr->dispatcher = dispatcher;
            promise = new_upromise_promise(dispatcher->dispatcher, &Promise::common_body, ptr);
        }
        ~Promise()
        {
            if (promise)
                del_upromise_promise(promise);
        }
        Promise(const Promise &p)
        {
            promise = p.promise;
            dispatcher = p.dispatcher;
            promise->rc += 1;
        }
        Promise &operator=(const Promise &p)
        {
            promise = p.promise;
            dispatcher = p.dispatcher;
            promise->rc += 1;
            return *this;
        }
        Promise(Promise &&p)
        {
            promise = p.promise;
            p.promise = NULL;
            dispatcher = std::move(p.dispatcher);
        }
        Promise &operator=(Promise &&p)
        {
            promise = p.promise;
            p.promise = NULL;
            dispatcher = std::move(p.dispatcher);
            return *this;
        }

        Promise then(CallbackFn onFulfilled, CallbackFn onRejected = nullptr) const
        {
            if (!promise)
                throw std::runtime_error("uninitialized promise");
            using T = std::variant<std::monostate, CallbackFn, ThenableCallbackFn>;
            return then_impl(onFulfilled ? T(onFulfilled) : T{}, onRejected ? T(onRejected) : T{});
        }

        Promise then(ThenableCallbackFn onFulfilled, CallbackFn onRejected = nullptr) const
        {
            if (!promise)
                throw std::runtime_error("uninitialized promise");
            using T = std::variant<std::monostate, CallbackFn, ThenableCallbackFn>;
            return then_impl(onFulfilled ? T(onFulfilled) : T{}, onRejected ? T(onRejected) : T{});
        }

        Promise then(CallbackFn onFulfilled, ThenableCallbackFn onRejected) const
        {
            if (!promise)
                throw std::runtime_error("uninitialized promise");
            using T = std::variant<std::monostate, CallbackFn, ThenableCallbackFn>;
            return then_impl(onFulfilled ? T(onFulfilled) : T{}, onRejected ? T(onRejected) : T{});
        }

        Promise then(ThenableCallbackFn onFulfilled, ThenableCallbackFn onRejected) const
        {
            if (!promise)
                throw std::runtime_error("uninitialized promise");
            using T = std::variant<std::monostate, CallbackFn, ThenableCallbackFn>;
            return then_impl(onFulfilled ? T(onFulfilled) : T{}, onRejected ? T(onRejected) : T{});
        }

        template <typename T2 = void *, std::enable_if_t<!std::is_same_v<T2, Thenable::Ptr>, int> = 0>
        Promise then(std::function<Thenable::Ptr(void *)> onFulfilled, std::function<T2(void *)> onRejected = nullptr) const
        {
            using T = std::variant<std::monostate, CallbackFn, ThenableCallbackFn>;
            auto dispatcher = this->dispatcher;
            return then_impl(
                T([=](void *value) -> Promise
                  {
                    auto thenable = onFulfilled(value);
                    return Promise(
                        dispatcher,
                        [=](ResolveNotifyFn resolve, NotifyFn reject) {
                            thenable->then(resolve, reject);
                    }); }),
                onRejected ? T(onRejected) : T{});
        }

        template <typename T1 = void *, std::enable_if_t<!std::is_same_v<T1, Thenable::Ptr>, int> = 0>
        Promise then(std::function<T1(void *)> onFulfilled, std::function<Thenable::Ptr(void *)> onRejected) const
        {
            using T = std::variant<std::monostate, CallbackFn, ThenableCallbackFn>;
            auto dispatcher = this->dispatcher;
            return then_impl(
                onFulfilled ? T(onFulfilled) : T{},
                T([=](void *value) -> Promise
                  {
                    auto thenable = onRejected(value);
                    return Promise(
                        dispatcher,
                        [=](ResolveNotifyFn resolve, NotifyFn reject) {
                            thenable->then(resolve, reject);
                    }); }));
        }

        Promise then(std::function<Thenable::Ptr(void *)> onFulfilled, std::function<Thenable::Ptr(void *)> onRejected) const
        {
            using T = std::variant<std::monostate, CallbackFn, ThenableCallbackFn>;
            auto dispatcher = this->dispatcher;
            return then_impl(
                T([=](void *value) -> Promise
                  {
                    auto thenable = onFulfilled(value);
                    return Promise(
                        dispatcher,
                        [=](ResolveNotifyFn resolve, NotifyFn reject) {
                            thenable->then(resolve, reject);
                    }); }),
                T([=](void *value) -> Promise
                  {
                    auto thenable = onRejected(value);
                    return Promise(
                        dispatcher,
                        [=](ResolveNotifyFn resolve, NotifyFn reject) {
                            thenable->then(resolve, reject);
                    }); }));
        }

    private:
        Promise then_impl(std::variant<std::monostate, CallbackFn, ThenableCallbackFn> onFulfilled = {}, std::variant<std::monostate, CallbackFn, ThenableCallbackFn> onRejected = {}) const
        {
            if (!promise)
                throw std::runtime_error("uninitialized promise");
            common_then_ctx *ctx = new common_then_ctx();
            ctx->onFulfilled = onFulfilled;
            ctx->onRejected = onRejected;
            upromise_promise_t *new_promise = NULL;
            if (onFulfilled.index() != 2 && onRejected.index() != 2)
                new_promise = upromise_promise_then(promise, ctx, onFulfilled.index() != 0 ? &Promise::common_fulfilled : nullptr, onRejected.index() != 0 ? &Promise::common_rejected : nullptr);
            else if (onFulfilled.index() == 2 && onRejected.index() != 2)
                new_promise = upromise_promise_then_thenable_common(promise, ctx, &Promise::common_fulfilled_thenable, onRejected.index() == 0 ? &Promise::common_rejected : nullptr);
            else if (onFulfilled.index() != 2 && onRejected.index() == 2)
                new_promise = upromise_promise_then_common_thenable(promise, ctx, onFulfilled.index() != 0 ? &Promise::common_fulfilled : nullptr, &Promise::common_rejected_thenable);
            else if (onFulfilled.index() == 2 && onRejected.index() == 2)
                new_promise = upromise_promise_then_thenable(promise, ctx, &Promise::common_fulfilled_thenable, &Promise::common_rejected_thenable);
            return Promise(dispatcher, new_promise);
        }

        struct Notifier
        {
            upromise_promise_t *promise;
            std::shared_ptr<Dispatcher> dispatcher;
            Notifier(upromise_promise_t *promise, std::shared_ptr<Dispatcher> dispatcher) : promise(promise), dispatcher(dispatcher) {}
            void operator()(void *data)
            {
                reject_upromise_promise(promise, data);
            }
        };

        struct ResolveNotifier
        {
            upromise_promise_t *promise;
            std::shared_ptr<Dispatcher> dispatcher;
            ResolveNotifier(upromise_promise_t *promise, std::shared_ptr<Dispatcher> dispatcher) : promise(promise), dispatcher(dispatcher) {}
            void operator()(Resolvable data);
        };

        static void common_body(upromise_promise_t *promise, void *ctx_raw)
        {
            BodyContext *ctx = (BodyContext *)ctx_raw;
            try
            {
                ctx->fn(ResolveNotifier(promise, ctx->dispatcher), Notifier(promise, ctx->dispatcher));
            }
            catch (Error err)
            {
                reject_upromise_promise(promise, err.err);
            }
            del_upromise_promise(promise);
            delete ctx;
        }

        struct common_then_ctx
        {
            std::variant<std::monostate, CallbackFn, ThenableCallbackFn> onFulfilled;
            std::variant<std::monostate, CallbackFn, ThenableCallbackFn> onRejected;
        };

        static void *common_fulfilled(void *data, void **error, void *ctx_raw)
        {
            common_then_ctx *ctx = (common_then_ctx *)ctx_raw;
            try
            {
                auto ret = std::get<1>(ctx->onFulfilled)(data);
                delete ctx;
                return ret;
            }
            catch (Error err)
            {
                *error = err.err;
            }
            return nullptr;
        }

        static void *common_rejected(void *data, void **error, void *ctx_raw)
        {
            common_then_ctx *ctx = (common_then_ctx *)ctx_raw;
            try
            {
                auto ret = std::get<1>(ctx->onRejected)(data);
                delete ctx;
                return ret;
            }
            catch (Error err)
            {
                *error = err.err;
            }
            return nullptr;
        }

        static upromise_promise_t *common_fulfilled_thenable(void *data, void **error, void *ctx_raw)
        {
            common_then_ctx *ctx = (common_then_ctx *)ctx_raw;
            try
            {
                auto ret = std::get<2>(ctx->onFulfilled)(data);
                delete ctx;
                auto promise = ret.promise;
                ret.promise = NULL;
                return promise;
            }
            catch (Error err)
            {
                *error = err.err;
            }
            return nullptr;
        }

        static upromise_promise_t *common_rejected_thenable(void *data, void **error, void *ctx_raw)
        {
            common_then_ctx *ctx = (common_then_ctx *)ctx_raw;
            try
            {
                auto ret = std::get<2>(ctx->onRejected)(data);
                delete ctx;
                auto promise = ret.promise;
                ret.promise = NULL;
                return promise;
            }
            catch (Error err)
            {
                *error = err.err;
            }
            return nullptr;
        }
    };

    inline void Promise::ResolveNotifier::operator()(Resolvable data)
    {
        switch (data.index())
        {
        case 0:
            resolve_upromise_promise(promise, std::get<0>(data));
            break;
        case 1:
        {
            auto &it = std::get<1>(data);
            auto value = it.promise;
            it.promise = nullptr;
            resolve_upromise_promise_thenable(promise, value);
            break;
        }
        case 2:
        {
            auto temp = Promise(
                dispatcher,
                [=](ResolveNotifyFn resolve, NotifyFn reject)
                {
                    std::get<2>(data)->then(resolve, reject);
                });
            auto value = temp.promise;
            temp.promise = nullptr;
            resolve_upromise_promise_thenable(promise, value);
            break;
        }
        default:
            break;
        }
    }
}
#endif

#endif
