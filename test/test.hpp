#ifndef _UPROMISE_TEST_HPP_
#define _UPROMISE_TEST_HPP_

#include <upromise/upromise.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <queue>
#include <thread>

class Adapter
{
    std::shared_ptr<upromise::Dispatcher> dispatcher;

public:
    Adapter(std::shared_ptr<upromise::Dispatcher> dispatcher) : dispatcher(dispatcher) {}
    upromise::Promise resolved(std::variant<void *, upromise::Promise, upromise::Thenable::Ptr> value = nullptr) const
    {
        return upromise::Promise(
            dispatcher,
            [=](upromise::Promise::ResolveNotifyFn resolve, upromise::Promise::NotifyFn reject)
            {
                resolve(value);
            });
    }
    upromise::Promise rejected(void *value = nullptr) const
    {
        return upromise::Promise(
            dispatcher,
            [=](upromise::Promise::ResolveNotifyFn resolve, upromise::Promise::NotifyFn reject)
            {
                reject(value);
            });
    }

    class Defer
    {
    private:
        upromise::Promise::ResolveNotifyFn resolveFn;
        upromise::Promise::NotifyFn rejectFn;
        friend class Adapter;

    public:
        Defer() {}
        Defer(const upromise::Promise &promise) : promise(promise) {}
        upromise::Promise promise;
        void resolve(std::variant<void *, upromise::Promise, upromise::Thenable::Ptr> data) const { resolveFn(data); }
        void reject(void *data) const { rejectFn(data); }
    };

    Defer deferred() const
    {
        upromise::Promise::ResolveNotifyFn resolveFn;
        upromise::Promise::NotifyFn rejectFn;
        auto defer = Defer(upromise::Promise(
            dispatcher,
            [&](upromise::Promise::ResolveNotifyFn resolve, upromise::Promise::NotifyFn reject)
            {
                resolveFn = resolve;
                rejectFn = reject;
            }));
        defer.resolveFn = resolveFn;
        defer.rejectFn = rejectFn;
        return defer;
    }
};

struct EventLoop
{
    std::shared_ptr<upromise::Dispatcher> dispatcher;
    std::queue<std::function<void()>> event_queue;
    std::mutex queue_lock;
    std::condition_variable cond;
    std::atomic<size_t> waiting;
    EventLoop() : dispatcher(std::make_shared<upromise::Dispatcher>()), waiting(0) {}
    ~EventLoop()
    {
        INFO((uintptr_t)this);
    }

    void run()
    {
        static void *event_promise = (void *)"event promise";
        while (true)
        {
            dispatcher->run();
            auto debug = waiting.load();
            if (waiting.load() == 0)
                break;
            bool keep = true;
            while (keep)
            {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queue_lock);
                    if (event_queue.empty())
                        cond.wait(lock);
                    task = event_queue.front();
                    event_queue.pop();
                    keep = event_queue.size() > 0;
                    waiting.fetch_sub(1);
                }
                upromise::Promise(
                    dispatcher,
                    [](upromise::Promise::NotifyFn resolve, upromise::Promise::NotifyFn)
                    {
                        resolve(event_promise);
                    })
                    .then(
                        [=](void *) -> void *
                        {
                            task();
                            return nullptr;
                        });
            }
        }
    }
};

inline std::function<void(std::function<void()> fn, std::chrono::duration<double> duration)> init_timeout(EventLoop *loop)
{
    return [=](std::function<void()> fn, auto duration)
    {
        loop->waiting.fetch_add(1);
        std::chrono::duration<double> real_duration = std::chrono::duration_cast<std::chrono::duration<double>>(duration);
        auto _fn = fn;
        std::thread(
            [=]()
            {
                std::this_thread::sleep_for(real_duration);
                {
                    std::lock_guard<std::mutex> lock(loop->queue_lock);
                    loop->event_queue.push(_fn);
                }
                loop->cond.notify_one();
            })
            .detach();
    };
}

struct specify
{
    std::shared_ptr<bool> value;
    specify() : value(std::make_shared<bool>(false)) {}
    void operator()() const { *value = true; }
};

#define PROLOGUE                                   \
    auto event_loop = EventLoop();                 \
    auto adapter = Adapter(event_loop.dispatcher); \
    auto setTimeout = init_timeout(&event_loop)
#define EPILOGUE \
    event_loop.run()

#define SPECIFY_BEGIN \
    specify done;
#define SPECIFY_END   \
    event_loop.run(); \
    CHECK(*done.value == true)

#define Bool std::make_shared<bool>
#define Int std::make_shared<int>
#define IntArray(...) std::make_shared<std::vector<int>>(std::vector<int>{__VA_ARGS__})

using namespace std::chrono_literals;

auto inline null()
{
    return upromise::Promise::null;
}

#define testFulfilled(value, var, ...)                                       \
    {                                                                        \
        SECTION("already-fulfilled")                                         \
        {                                                                    \
            SPECIFY_BEGIN;                                                   \
            [=](upromise::Promise var) __VA_ARGS__(adapter.resolved(value)); \
            SPECIFY_END;                                                     \
        }                                                                    \
                                                                             \
        SECTION("immediately-fulfilled")                                     \
        {                                                                    \
            SPECIFY_BEGIN;                                                   \
            auto d = adapter.deferred();                                     \
            [=](upromise::Promise var) __VA_ARGS__(d.promise);               \
            d.resolve(value);                                                \
            SPECIFY_END;                                                     \
        }                                                                    \
                                                                             \
        SECTION("eventually-fulfilled")                                      \
        {                                                                    \
            SPECIFY_BEGIN;                                                   \
            auto d = adapter.deferred();                                     \
            [=](upromise::Promise var) __VA_ARGS__(d.promise);               \
            setTimeout([=]() { d.resolve(value); }, 50ms);                   \
            SPECIFY_END;                                                     \
        }                                                                    \
    }

#define testRejected(value, var, ...)                                        \
    {                                                                        \
        SECTION("already-fulfilled")                                         \
        {                                                                    \
            SPECIFY_BEGIN;                                                   \
            [=](upromise::Promise var) __VA_ARGS__(adapter.rejected(value)); \
            SPECIFY_END;                                                     \
        }                                                                    \
                                                                             \
        SECTION("immediately-fulfilled")                                     \
        {                                                                    \
            SPECIFY_BEGIN;                                                   \
            auto d = adapter.deferred();                                     \
            [=](upromise::Promise var) __VA_ARGS__(d.promise);               \
            d.reject(value);                                                 \
            SPECIFY_END;                                                     \
        }                                                                    \
                                                                             \
        SECTION("eventually-fulfilled")                                      \
        {                                                                    \
            SPECIFY_BEGIN;                                                   \
            auto d = adapter.deferred();                                     \
            [=](upromise::Promise var) __VA_ARGS__(d.promise);               \
            setTimeout([=]() { d.reject(value); }, 50ms);                    \
            SPECIFY_END;                                                     \
        }                                                                    \
    }

namespace helper
{
    static inline void *other = (void *)"other";
    inline auto thenables(Adapter adapter, std::function<void(std::function<void()> fn, std::chrono::duration<double> duration)> setTimeout)
    {
        struct Thenables
        {
            using Value = std::variant<void *, upromise::Promise, upromise::Thenable::Ptr>;
            std::map<std::string, std::function<Value(Value)>> fulfilled;
            std::map<std::string, std::function<Value(void *)>> rejected;
            Thenables(Adapter adapter, std::function<void(std::function<void()> fn, std::chrono::duration<double> duration)> setTimeout)
            {
                fulfilled = std::map<std::string, std::function<Value(Value)>>{
                    {
                        "a synchronously-fulfilled custom thenable",
                        [=](Value value)
                        {
                            return std::make_shared<upromise::Thenable>(
                                [=](upromise::Thenable::ResolveNotifyFn onFulfilled, upromise::Thenable::NotifyFn)
                                {
                                    onFulfilled(value);
                                });
                        },
                    },
                    {
                        "an asynchronously-fulfilled custom thenable",
                        [=](Value value)
                        {
                            return std::make_shared<upromise::Thenable>(
                                [=](upromise::Thenable::ResolveNotifyFn onFulfilled, upromise::Thenable::NotifyFn)
                                {
                                    setTimeout(
                                        [=]()
                                        {
                                            onFulfilled(value);
                                        },
                                        0ms);
                                });
                        },
                    },
                    // "a synchronously-fulfilled one-time thenable"
                    // `then` in C++ is not mutable
                    {
                        "a thenable that tries to fulfill twice",
                        [=](Value value)
                        {
                            return std::make_shared<upromise::Thenable>(
                                [=](upromise::Thenable::ResolveNotifyFn onFulfilled, upromise::Thenable::NotifyFn)
                                {
                                    onFulfilled(value);
                                    onFulfilled(other);
                                });
                        }},
                    {
                        "a thenable that fulfills but then throws",
                        [=](Value value)
                        {
                            return std::make_shared<upromise::Thenable>(
                                [=](upromise::Thenable::ResolveNotifyFn onFulfilled, upromise::Thenable::NotifyFn)
                                {
                                    onFulfilled(value);
                                    throw upromise::Promise::Error{other};
                                });
                        },
                    },
                    {
                        "an already-fulfilled promise",
                        [=](Value value)
                        {
                            return adapter.resolved(value);
                        },
                    },
                    {
                        "an eventually-fulfilled promise",
                        [=](Value value)
                        {
                            auto d = adapter.deferred();
                            setTimeout(
                                [=]()
                                {
                                    d.resolve(value);
                                },
                                50ms);
                            return d.promise;
                        },
                    },
                };

                rejected = std::map<std::string, std::function<Value(void *)>>{
                    {
                        "a synchronously-rejected custom thenable",
                        [=](void *reason)
                        {
                            return std::make_shared<upromise::Thenable>(
                                [=](upromise::Thenable::ResolveNotifyFn, upromise::Thenable::NotifyFn onRejected)
                                {
                                    onRejected(reason);
                                });
                        },
                    },
                    {
                        "an asynchronously-rejected custom thenable",
                        [=](void *reason)
                        {
                            return std::make_shared<upromise::Thenable>(
                                [=](upromise::Thenable::ResolveNotifyFn, upromise::Thenable::NotifyFn onRejected)
                                {
                                    setTimeout(
                                        [=]()
                                        {
                                            onRejected(reason);
                                        },
                                        0ms);
                                });
                        },
                    },
                    // "a synchronously-rejected one-time thenable"
                    // `then` in C++ is not mutable
                    {
                        "a thenable that immediately throws in `then`",
                        [=](void *reason)
                        {
                            return std::make_shared<upromise::Thenable>(
                                [=](upromise::Thenable::ResolveNotifyFn, upromise::Thenable::NotifyFn)
                                {
                                    throw upromise::Promise::Error{reason};
                                });
                        },
                    },
                    // "an object with a throwing `then` accessor"
                    // `then` in C++ is not mutable
                    {
                        "an already-rejected promise",
                        [=](void *reason)
                        {
                            return adapter.rejected(reason);
                        },
                    },
                    {"an eventually-rejected promise",
                     [=](void *reason)
                     {
                         auto d = adapter.deferred();
                         setTimeout(
                             [=]()
                             {
                                 d.reject(reason);
                             },
                             50ms);
                         return d.promise;
                     }},
                };
            }
        };
        return Thenables(adapter, setTimeout);
    }
}

#endif
