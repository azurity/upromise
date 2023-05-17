#include <catch2/catch.hpp>
#include "test.hpp"
#include "sinon.hpp"

void *dummy = (void *)"dummy";

TEST_CASE("2.1.2.1: When fulfilled, a promise: must not transition to any other state.", "[2.1.2]")
{
    PROLOGUE;

    testFulfilled(dummy, promise, {
        auto onFulfilledCalled = Bool(false);

        promise.then(
            [=](void *) -> void *
            {
                *onFulfilledCalled = true;
                return nullptr;
            },
            [=](void *) -> void *
            {
                CHECK(*onFulfilledCalled == false);
                done();
                return nullptr;
            });

        setTimeout(done, 100ms);
    });

    SECTION("trying to fulfill then immediately reject")
    {
        SPECIFY_BEGIN;
        auto d = adapter.deferred();
        auto onFulfilledCalled = Bool(false);

        d.promise.then(
            [=](void *) -> void *
            {
                *onFulfilledCalled = true;
                return nullptr;
            },
            [=](void *) -> void *
            {
                CHECK(*onFulfilledCalled == false);
                done();
                return nullptr;
            });

        d.resolve(dummy);
        d.reject(dummy);
        setTimeout(done, 100ms);
        SPECIFY_END;
    }

    SECTION("trying to fulfill then reject, delayed")
    {
        SPECIFY_BEGIN;
        auto d = adapter.deferred();
        auto onFulfilledCalled = Bool(false);

        d.promise.then(
            [=](void *) -> void *
            {
                *onFulfilledCalled = true;
                return nullptr;
            },
            [=](void *) -> void *
            {
                CHECK(*onFulfilledCalled == false);
                done();
                return nullptr;
            });

        setTimeout(
            [=]()
            {
                d.resolve(dummy);
                d.reject(dummy);
            },
            50ms);
        setTimeout(done, 100ms);
        SPECIFY_END;
    }

    SECTION("trying to fulfill immediately then reject delayed")
    {
        SPECIFY_BEGIN;
        auto d = adapter.deferred();
        auto onFulfilledCalled = Bool(false);

        d.promise.then(
            [=](void *) -> void *
            {
                *onFulfilledCalled = true;
                return nullptr;
            },
            [=](void *) -> void *
            {
                CHECK(*onFulfilledCalled == false);
                done();
                return nullptr;
            });

        d.resolve(dummy);
        setTimeout(
            [=]()
            {
                d.reject(dummy);
            },
            50ms);
        setTimeout(done, 100ms);
        SPECIFY_END;
    }

    EPILOGUE;
}

TEST_CASE("2.1.3.1: When rejected, a promise: must not transition to any other state.", "[2.1.3]")
{
    PROLOGUE;

    testRejected(dummy, promise, {
        auto onRejectedCalled = Bool(false);

        promise.then(
            [=](void *) -> void *
            {
                CHECK(*onRejectedCalled == false);
                done();
                return nullptr;
            },
            [=](void *) -> void *
            {
                *onRejectedCalled = true;
                return nullptr;
            });

        setTimeout(done, 100ms);
    });

    SECTION("trying to reject then immediately fulfill")
    {
        SPECIFY_BEGIN;
        auto d = adapter.deferred();
        auto onRejectedCalled = Bool(false);

        d.promise.then(
            [=](void *) -> void *
            {
                CHECK(*onRejectedCalled == false);
                done();
                return nullptr;
            },
            [=](void *) -> void *
            {
                *onRejectedCalled = true;
                return nullptr;
            });

        d.reject(dummy);
        d.resolve(dummy);
        setTimeout(done, 100ms);
        SPECIFY_END;
    }

    SECTION("trying to reject then fulfill, delayed")
    {
        SPECIFY_BEGIN;
        auto d = adapter.deferred();
        auto onRejectedCalled = Bool(false);

        d.promise.then(
            [=](void *) -> void *
            {
                CHECK(*onRejectedCalled == false);
                done();
                return nullptr;
            },
            [=](void *) -> void *
            {
                *onRejectedCalled = true;
                return nullptr;
            });

        setTimeout(
            [=]()
            {
                d.reject(dummy);
                d.resolve(dummy);
            },
            50ms);
        setTimeout(done, 100ms);
        SPECIFY_END;
    }

    SECTION("trying to reject immediately then fulfill delayed")
    {
        SPECIFY_BEGIN;
        auto d = adapter.deferred();
        auto onRejectedCalled = Bool(false);

        d.promise.then(
            [=](void *) -> void *
            {
                CHECK(*onRejectedCalled == false);
                done();
                return nullptr;
            },
            [=](void *) -> void *
            {
                *onRejectedCalled = true;
                return nullptr;
            });

        d.reject(dummy);
        setTimeout(
            [=]()
            {
                d.resolve(dummy);
            },
            50ms);
        setTimeout(done, 100ms);
        SPECIFY_END;
    }

    EPILOGUE;
}

TEST_CASE("2.2.1: Both `onFulfilled` and `onRejected` are optional arguments.", "[2.2.1]")
{
    // Typed language do not need type test.
}

void *sentinel = (void *)"sentinel";

TEST_CASE("2.2.2: If `onFulfilled` is a function,", "[2.2.2]")
{
    PROLOGUE;

    SECTION("2.2.2.1: it must be called after `promise` is fulfilled, with `promise`’s fulfillment value as its first argument.")
    {
        testFulfilled(sentinel, promise, {
            promise.then(
                [=](void *value) -> void *
                {
                    CHECK(value == sentinel);
                    done();
                    return nullptr;
                });
        });
    }

    SECTION("2.2.2.2: it must not be called before `promise` is fulfilled")
    {
        SECTION("fulfilled after a delay")
        {
            SPECIFY_BEGIN;
            auto d = adapter.deferred();
            auto isFulfilled = Bool(false);

            d.promise.then(
                [=](void *) -> void *
                {
                    CHECK(*isFulfilled == true);
                    done();
                    return nullptr;
                });

            setTimeout(
                [=]()
                {
                    d.resolve(dummy);
                    *isFulfilled = true;
                },
                50ms);
            SPECIFY_END;
        }

        SECTION("never fulfilled")
        {
            SPECIFY_BEGIN;
            auto d = adapter.deferred();
            auto onFulfilledCalled = Bool(false);

            d.promise.then(
                [=](void *) -> void *
                {
                    *onFulfilledCalled = true;
                    done();
                    return nullptr;
                });

            setTimeout(
                [=]()
                {
                    CHECK(*onFulfilledCalled == false);
                    done();
                },
                150ms);
            SPECIFY_END;
        }
    }

    SECTION("2.2.2.3: it must not be called more than once.")
    {
        SECTION("already-fulfilled")
        {
            SPECIFY_BEGIN;
            auto timesCalled = Int(0);

            adapter.resolved(dummy).then(
                [=](void *) -> void *
                {
                    CHECK(++*timesCalled == 1);
                    done();
                    return nullptr;
                });
            SPECIFY_END;
        }

        SECTION("trying to fulfill a pending promise more than once, immediately")
        {
            SPECIFY_BEGIN;
            auto d = adapter.deferred();
            auto timesCalled = Int(0);

            d.promise.then(
                [=](void *) -> void *
                {
                    CHECK(++*timesCalled == 1);
                    done();
                    return nullptr;
                });

            d.resolve(dummy);
            d.resolve(dummy);
            SPECIFY_END;
        }

        SECTION("trying to fulfill a pending promise more than once, delayed")
        {
            SPECIFY_BEGIN;
            auto d = adapter.deferred();
            auto timesCalled = Int(0);

            d.promise.then(
                [=](void *) -> void *
                {
                    CHECK(++*timesCalled == 1);
                    done();
                    return 0;
                });

            setTimeout(
                [=]()
                {
                    d.resolve(dummy);
                    d.resolve(dummy);
                },
                50ms);
            SPECIFY_END;
        }

        SECTION("trying to fulfill a pending promise more than once, immediately then delayed")
        {
            SPECIFY_BEGIN;
            auto d = adapter.deferred();
            auto timesCalled = Int(0);

            d.promise.then(
                [=](void *) -> void *
                {
                    CHECK(++*timesCalled == 1);
                    done();
                    return nullptr;
                });

            d.resolve(dummy);
            setTimeout(
                [=]()
                {
                    d.resolve(dummy);
                },
                50ms);
            SPECIFY_END;
        }

        SECTION("when multiple `then` calls are made, spaced apart in time")
        {
            SPECIFY_BEGIN;
            auto d = adapter.deferred();
            auto timesCalled = IntArray(0, 0, 0);

            d.promise.then(
                [=](void *) -> void *
                {
                    CHECK(++(*timesCalled)[0] == 1);
                    return nullptr;
                });

            setTimeout(
                [=]()
                {
                    d.promise.then(
                        [=](void *) -> void *
                        {
                            CHECK(++(*timesCalled)[1] == 1);
                            return nullptr;
                        });
                },
                50ms);

            setTimeout(
                [=]()
                {
                    d.promise.then(
                        [=](void *) -> void *
                        {
                            CHECK(++(*timesCalled)[2] == 1);
                            done();
                            return nullptr;
                        });
                },
                100ms);

            setTimeout(
                [=]()
                {
                    d.resolve(dummy);
                },
                150ms);
            SPECIFY_END;
        }

        SECTION("when `then` is interleaved with fulfillment")
        {
            SPECIFY_BEGIN;
            auto d = adapter.deferred();
            auto timesCalled = IntArray(0, 0);

            d.promise.then(
                [=](void *) -> void *
                {
                    CHECK(++(*timesCalled)[0] == 1);
                    return nullptr;
                });

            d.resolve(dummy);

            d.promise.then(
                [=](void *) -> void *
                {
                    CHECK(++(*timesCalled)[1] == 1);
                    done();
                    return nullptr;
                });
            SPECIFY_END;
        }
    }

    EPILOGUE;
}

TEST_CASE("2.2.3: If `onRejected` is a function,", "[2.2.3]")
{
    PROLOGUE;

    SECTION("2.2.3.1: it must be called after `promise` is rejected, with `promise`’s rejection reason as its first argument.")
    {
        testRejected(sentinel, promise, {
            promise.then(
                null(),
                [=](void *reason) -> void *
                {
                    CHECK(reason == sentinel);
                    done();
                    return nullptr;
                });
        });
    }

    SECTION("2.2.3.2: it must not be called before `promise` is rejected")
    {
        SECTION("rejected after a delay")
        {
            SPECIFY_BEGIN;
            auto d = adapter.deferred();
            auto isRejected = Bool(false);

            d.promise.then(
                null(),
                [=](void *) -> void *
                {
                    CHECK(*isRejected == true);
                    done();
                    return nullptr;
                });

            setTimeout(
                [=]()
                {
                    d.reject(dummy);
                    *isRejected = true;
                },
                50ms);
            SPECIFY_END;
        }

        SECTION("never rejected")
        {
            SPECIFY_BEGIN;
            auto d = adapter.deferred();
            auto onRejectedCalled = Bool(false);

            d.promise.then(
                null(),
                [=](void *) -> void *
                {
                    *onRejectedCalled = true;
                    done();
                    return nullptr;
                });

            setTimeout(
                [=]()
                {
                    CHECK(*onRejectedCalled == false);
                    done();
                },
                150ms);
            SPECIFY_END;
        }
    }

    SECTION("2.2.3.3: it must not be called more than once.")
    {
        SECTION("already-rejected")
        {
            SPECIFY_BEGIN;
            auto timesCalled = Int(0);

            adapter.rejected(dummy).then(
                null(),
                [=](void *) -> void *
                {
                    CHECK(++(*timesCalled) == 1);
                    done();
                    return nullptr;
                });
            SPECIFY_END;
        }

        SECTION("trying to reject a pending promise more than once, immediately")
        {
            SPECIFY_BEGIN;
            auto d = adapter.deferred();
            auto timesCalled = Int(0);

            d.promise.then(
                null(),
                [=](void *) -> void *
                {
                    CHECK(++(*timesCalled) == 1);
                    done();
                    return nullptr;
                });

            d.reject(dummy);
            d.reject(dummy);
            SPECIFY_END;
        }

        SECTION("trying to reject a pending promise more than once, delayed")
        {
            SPECIFY_BEGIN;
            auto d = adapter.deferred();
            auto timesCalled = Int(0);

            d.promise.then(
                null(),
                [=](void *) -> void *
                {
                    CHECK(++(*timesCalled) == 1);
                    done();
                    return nullptr;
                });

            setTimeout(
                [=]()
                {
                    d.reject(dummy);
                    d.reject(dummy);
                },
                50ms);
            SPECIFY_END;
        }

        SECTION("trying to reject a pending promise more than once, immediately then delayed")
        {
            SPECIFY_BEGIN;
            auto d = adapter.deferred();
            auto timesCalled = Int(0);

            d.promise.then(
                null(),
                [=](void *) -> void *
                {
                    CHECK(++(*timesCalled) == 1);
                    done();
                    return nullptr;
                });

            d.reject(dummy);
            setTimeout(
                [=]()
                {
                    d.reject(dummy);
                },
                50ms);
            SPECIFY_END;
        }

        SECTION("when multiple `then` calls are made, spaced apart in time")
        {
            SPECIFY_BEGIN;
            auto d = adapter.deferred();
            auto timesCalled = IntArray(0, 0, 0);

            d.promise.then(
                null(),
                [=](void *) -> void *
                {
                    CHECK(++(*timesCalled)[0] == 1);
                    return nullptr;
                });

            setTimeout(
                [=]()
                {
                    d.promise.then(
                        null(),
                        [=](void *) -> void *
                        {
                            CHECK(++(*timesCalled)[1] == 1);
                            return nullptr;
                        });
                },
                50ms);

            setTimeout(
                [=]()
                {
                    d.promise.then(
                        null(),
                        [=](void *) -> void *
                        {
                            CHECK(++(*timesCalled)[2] == 1);
                            done();
                            return nullptr;
                        });
                },
                100ms);

            setTimeout(
                [=]()
                {
                    d.reject(dummy);
                },
                150ms);
            SPECIFY_END;
        }

        SECTION("when `then` is interleaved with rejection")
        {
            SPECIFY_BEGIN;
            auto d = adapter.deferred();
            auto timesCalled = IntArray(0, 0);

            d.promise.then(
                null(),
                [=](void *) -> void *
                {
                    CHECK(++(*timesCalled)[0] == 1);
                    return nullptr;
                });

            d.reject(dummy);

            d.promise.then(
                null(),
                [=](void *) -> void *
                {
                    CHECK(++(*timesCalled)[1] == 1);
                    done();
                    return nullptr;
                });
            SPECIFY_END;
        }
    }

    EPILOGUE;
}

TEST_CASE("2.2.4: `onFulfilled` or `onRejected` must not be called until the execution context stack contains only platform code.", "[2.2.4]")
{
    PROLOGUE;

    SECTION("`then` returns before the promise becomes fulfilled or rejected")
    {
        testFulfilled(dummy, promise, {
            auto thenHasReturned = Bool(false);

            promise.then(
                [=](void *) -> void *
                {
                    CHECK(*thenHasReturned == true);
                    done();
                    return nullptr;
                });

            *thenHasReturned = true;
        });
        testRejected(dummy, promise, {
            auto thenHasReturned = Bool(false);

            promise.then(
                null(),
                [=](void *) -> void *
                {
                    CHECK(*thenHasReturned == true);
                    done();
                    return nullptr;
                });

            *thenHasReturned = true;
        });
    }

    SECTION("Clean-stack execution ordering tests (fulfillment case)")
    {
        SECTION("when `onFulfilled` is added immediately before the promise is fulfilled")
        {
            auto d = adapter.deferred();
            auto onFulfilledCalled = Bool(false);

            d.promise.then(
                [=](void *) -> void *
                {
                    *onFulfilledCalled = true;
                    return nullptr;
                });

            d.resolve(dummy);

            CHECK(*onFulfilledCalled == false);
        }

        SECTION("when `onFulfilled` is added immediately after the promise is fulfilled")
        {
            auto d = adapter.deferred();
            auto onFulfilledCalled = Bool(false);

            d.resolve(dummy);

            d.promise.then(
                [=](void *) -> void *
                {
                    *onFulfilledCalled = true;
                    return nullptr;
                });

            CHECK(*onFulfilledCalled == false);
        }

        SECTION("when one `onFulfilled` is added inside another `onFulfilled`")
        {
            SPECIFY_BEGIN;
            auto promise = adapter.resolved();
            auto firstOnFulfilledFinished = Bool(false);

            promise.then(
                [=](void *) -> void *
                {
                    promise.then(
                        [=](void *) -> void *
                        {
                            CHECK(*firstOnFulfilledFinished == true);
                            done();
                            return nullptr;
                        });
                    *firstOnFulfilledFinished = true;
                    return nullptr;
                });
            SPECIFY_END;
        }

        SECTION("when `onFulfilled` is added inside an `onRejected`")
        {
            SPECIFY_BEGIN;
            auto promise = adapter.rejected();
            auto promise2 = adapter.resolved();
            auto firstOnRejectedFinished = Bool(false);

            promise.then(
                null(),
                [=](void *) -> void *
                {
                    promise2.then(
                        [=](void *) -> void *
                        {
                            CHECK(*firstOnRejectedFinished == true);
                            done();
                            return nullptr;
                        });
                    *firstOnRejectedFinished = true;
                    return nullptr;
                });
            SPECIFY_END;
        }

        SECTION("when the promise is fulfilled asynchronously")
        {
            SPECIFY_BEGIN;
            auto d = adapter.deferred();
            auto firstStackFinished = Bool(false);

            setTimeout(
                [=]()
                {
                    d.resolve(dummy);
                    *firstStackFinished = true;
                },
                0ms);

            d.promise.then(
                [=](void *) -> void *
                {
                    CHECK(*firstStackFinished == true);
                    done();
                    return nullptr;
                });
            SPECIFY_END;
        }
    }

    SECTION("Clean-stack execution ordering tests (rejection case)")
    {
        SECTION("when `onRejected` is added immediately before the promise is rejected")
        {
            auto d = adapter.deferred();
            auto onRejectedCalled = Bool(false);

            d.promise.then(
                null(),
                [=](void *) -> void *
                {
                    *onRejectedCalled = true;
                    return nullptr;
                });

            d.reject(dummy);

            CHECK(*onRejectedCalled == false);
        }

        SECTION("when `onRejected` is added immediately after the promise is rejected")
        {
            auto d = adapter.deferred();
            auto onRejectedCalled = Bool(false);

            d.reject(dummy);

            d.promise.then(
                null(),
                [=](void *) -> void *
                {
                    *onRejectedCalled = true;
                    return nullptr;
                });

            CHECK(*onRejectedCalled == false);
        }

        SECTION("when `onRejected` is added inside an `onFulfilled`")
        {
            SPECIFY_BEGIN;
            auto promise = adapter.resolved();
            auto promise2 = adapter.rejected();
            auto firstOnFulfilledFinished = Bool(false);

            promise.then(
                [=](void *) -> void *
                {
                    promise2.then(
                        null(),
                        [=](void *) -> void *
                        {
                            CHECK(*firstOnFulfilledFinished == true);
                            done();
                            return nullptr;
                        });
                    *firstOnFulfilledFinished = true;
                    return nullptr;
                });
            SPECIFY_END;
        }

        SECTION("when one `onRejected` is added inside another `onRejected`")
        {
            SPECIFY_BEGIN;
            auto promise = adapter.rejected();
            auto firstOnRejectedFinished = Bool(false);

            promise.then(
                null(),
                [=](void *) -> void *
                {
                    promise.then(
                        null(),
                        [=](void *) -> void *
                        {
                            CHECK(*firstOnRejectedFinished == true);
                            done();
                            return nullptr;
                        });
                    *firstOnRejectedFinished = true;
                    return nullptr;
                });
            SPECIFY_END;
        }

        SECTION("when the promise is rejected asynchronously")
        {
            SPECIFY_BEGIN;
            auto d = adapter.deferred();
            auto firstStackFinished = Bool(false);

            setTimeout([=]()
                       {
                d.reject(dummy);
                *firstStackFinished = true; },
                       0ms);

            d.promise.then(
                null(),
                [=](void *) -> void *
                {
                    CHECK(*firstStackFinished == true);
                    done();
                    return nullptr;
                });
            SPECIFY_END;
        }
    }

    EPILOGUE;
}

TEST_CASE("2.2.5 `onFulfilled` and `onRejected` must be called as functions (i.e. with no `this` value).", "[2.2.5]")
{
    // C++ do not have strict mode & sloppy mode
}

void *other = (void *)"other";
void *sentinel2 = (void *)"sentinel2";
void *sentinel3 = (void *)"sentinel3";

auto callbackAggregator(int times, std::function<void()> ultimateCallback)
{
    auto soFar = Int(0);
    return [=]()
    {
        if (++(*soFar) == times)
        {
            ultimateCallback();
        }
    };
}

TEST_CASE("2.2.6: `then` may be called multiple times on the same promise.", "[2.2.6]")
{
    PROLOGUE;
    using Callback = upromise::Promise::CallbackFn;

    SECTION("2.2.6.1: If/when `promise` is fulfilled, all respective `onFulfilled` callbacks must execute in the "
            "order of their originating calls to `then`.")
    {
        SECTION("multiple boring fulfillment handlers")
        {
            testFulfilled(sentinel, promise, {
                auto handler1 = sinon::stub().returns(other);
                auto handler2 = sinon::stub().returns(other);
                auto handler3 = sinon::stub().returns(other);

                auto spy = sinon::spy<void *>();
                promise.then((Callback)handler1, (Callback)spy);
                promise.then((Callback)handler2, (Callback)spy);
                promise.then((Callback)handler3, (Callback)spy);

                promise.then(
                    [=](void *value) -> void *
                    {
                        CHECK(value == sentinel);

                        CHECK(sinon::asserts::calledWith(handler1, sinon::match::same(sentinel)));
                        CHECK(sinon::asserts::calledWith(handler2, sinon::match::same(sentinel)));
                        CHECK(sinon::asserts::calledWith(handler3, sinon::match::same(sentinel)));
                        CHECK(sinon::asserts::notCalled(spy));

                        done();
                        return nullptr;
                    });
            });
        }

        SECTION("multiple fulfillment handlers, one of which throws")
        {
            testFulfilled(sentinel, promise, {
                auto handler1 = sinon::stub().returns(other);
                auto handler2 = sinon::stub().throws(upromise::Promise::Error{other});
                auto handler3 = sinon::stub().returns(other);

                auto spy = sinon::spy<void *>();
                promise.then((Callback)handler1, (Callback)spy);
                promise.then((Callback)handler2, (Callback)spy);
                promise.then((Callback)handler3, (Callback)spy);

                promise.then(
                    [=](void *value) -> void *
                    {
                        CHECK(value == sentinel);

                        CHECK(sinon::asserts::calledWith(handler1, sinon::match::same(sentinel)));
                        CHECK(sinon::asserts::calledWith(handler2, sinon::match::same(sentinel)));
                        CHECK(sinon::asserts::calledWith(handler3, sinon::match::same(sentinel)));
                        CHECK(sinon::asserts::notCalled(spy));

                        done();
                        return nullptr;
                    });
            });
        }

        SECTION("results in multiple branching chains with their own fulfillment values")
        {
            testFulfilled(dummy, promise, {
                auto semiDone = callbackAggregator(3, done);

                promise.then(
                           [=](void *) -> void *
                           {
                               return sentinel;
                           })
                    .then(
                        [=](void *value) -> void *
                        {
                            CHECK(value == sentinel);
                            semiDone();
                            return nullptr;
                        });

                promise.then(
                           [=](void *) -> void *
                           {
                               throw upromise::Promise::Error{sentinel2};
                           })
                    .then(
                        null(),
                        [=](void *reason) -> void *
                        {
                            CHECK(reason == sentinel2);
                            semiDone();
                            return nullptr;
                        });

                promise.then(
                           [=](void *) -> void *
                           {
                               return sentinel3;
                           })
                    .then(
                        [=](void *value) -> void *
                        {
                            CHECK(value == sentinel3);
                            semiDone();
                            return nullptr;
                        });
            });
        }

        SECTION("`onFulfilled` handlers are called in the original order")
        {
            testFulfilled(dummy, promise, {
                auto handler1 = sinon::spy(sinon::detail::BlankHolder<void *>());
                auto handler2 = sinon::spy(sinon::detail::BlankHolder<void *>());
                auto handler3 = sinon::spy(sinon::detail::BlankHolder<void *>());

                promise.then((Callback)handler1);
                promise.then((Callback)handler2);
                promise.then((Callback)handler3);

                promise.then(
                    [=](void *) -> void *
                    {
                        CHECK(sinon::asserts::callOrder(handler1, handler2, handler3));
                        done();
                        return nullptr;
                    });
            });

            SECTION("even when one handler is added inside another handler")
            {
                testFulfilled(dummy, promise, {
                    auto handler1 = sinon::spy(sinon::detail::BlankHolder<void *>());
                    auto handler2 = sinon::spy(sinon::detail::BlankHolder<void *>());
                    auto handler3 = sinon::spy(sinon::detail::BlankHolder<void *>());

                    promise.then(
                        [=](void *) -> void *
                        {
                            ((upromise::Promise::CallbackFn)handler1)(nullptr);
                            promise.then((Callback)handler3);
                            return nullptr;
                        });
                    promise.then((Callback)handler2);

                    promise.then(
                        [=](void *) -> void *
                        {
                            // Give implementations a bit of extra time to flush their internal queue, if necessary.
                            setTimeout(
                                [=]()
                                {
                                    CHECK(sinon::asserts::callOrder(handler1, handler2, handler3));
                                    done();
                                    return nullptr;
                                },
                                15ms);
                            return nullptr;
                        });
                });
            }
        }
    }

    SECTION("2.2.6.2: If/when `promise` is rejected, all respective `onRejected` callbacks must execute in the "
            "order of their originating calls to `then`.")
    {
        SECTION("multiple boring rejection handlers")
        {
            testRejected(sentinel, promise, {
                auto handler1 = sinon::stub().returns(other);
                auto handler2 = sinon::stub().returns(other);
                auto handler3 = sinon::stub().returns(other);

                auto spy = sinon::spy<void *>();
                promise.then((Callback)spy, (Callback)handler1);
                promise.then((Callback)spy, (Callback)handler2);
                promise.then((Callback)spy, (Callback)handler3);

                promise.then(
                    null(),
                    [=](void *reason) -> void *
                    {
                        CHECK(reason == sentinel);

                        CHECK(sinon::asserts::calledWith(handler1, sinon::match::same(sentinel)));
                        CHECK(sinon::asserts::calledWith(handler2, sinon::match::same(sentinel)));
                        CHECK(sinon::asserts::calledWith(handler3, sinon::match::same(sentinel)));
                        CHECK(sinon::asserts::notCalled(spy));

                        done();
                        return nullptr;
                    });
            });
        }

        SECTION("multiple rejection handlers, one of which throws")
        {
            testRejected(sentinel, promise, {
                auto handler1 = sinon::stub().returns(other);
                auto handler2 = sinon::stub().throws(upromise::Promise::Error{other});
                auto handler3 = sinon::stub().returns(other);

                auto spy = sinon::spy<void *>();
                promise.then((Callback)spy, (Callback)handler1);
                promise.then((Callback)spy, (Callback)handler2);
                promise.then((Callback)spy, (Callback)handler3);

                promise.then(
                    null(),
                    [=](void *reason) -> void *
                    {
                        CHECK(reason == sentinel);

                        CHECK(sinon::asserts::calledWith(handler1, sinon::match::same(sentinel)));
                        CHECK(sinon::asserts::calledWith(handler2, sinon::match::same(sentinel)));
                        CHECK(sinon::asserts::calledWith(handler3, sinon::match::same(sentinel)));
                        CHECK(sinon::asserts::notCalled(spy));

                        done();
                        return nullptr;
                    });
            });
        }

        SECTION("results in multiple branching chains with their own fulfillment values")
        {
            testRejected(sentinel, promise, {
                auto semiDone = callbackAggregator(3, done);

                promise.then(
                           null(),
                           [=](void *reason) -> void *
                           {
                               return sentinel;
                           })
                    .then(
                        [=](void *value) -> void *
                        {
                            CHECK(value == sentinel);
                            semiDone();
                            return nullptr;
                        });

                promise.then(
                           null(),
                           [=](void *reason) -> void *
                           {
                               throw upromise::Promise::Error{sentinel2};
                           })
                    .then(
                        null(),
                        [=](void *reason) -> void *
                        {
                            CHECK(reason == sentinel2);
                            semiDone();
                            return nullptr;
                        });

                promise.then(
                           null(),
                           [=](void *reason) -> void *
                           {
                               return sentinel3;
                           })
                    .then(
                        [=](void *value) -> void *
                        {
                            CHECK(value == sentinel3);
                            semiDone();
                            return nullptr;
                        });
            });
        }

        SECTION("`onRejected` handlers are called in the original order")
        {
            testRejected(dummy, promise, {
                auto handler1 = sinon::spy(sinon::detail::BlankHolder<void *>());
                auto handler2 = sinon::spy(sinon::detail::BlankHolder<void *>());
                auto handler3 = sinon::spy(sinon::detail::BlankHolder<void *>());

                promise.then(null(), (Callback)handler1);
                promise.then(null(), (Callback)handler2);
                promise.then(null(), (Callback)handler3);

                promise.then(
                    null(),
                    [=](void *) -> void *
                    {
                        CHECK(sinon::asserts::callOrder(handler1, handler2, handler3));
                        done();
                        return nullptr;
                    });
            });

            SECTION("even when one handler is added inside another handler")
            {
                testRejected(dummy, promise, {
                    auto handler1 = sinon::spy(sinon::detail::BlankHolder<void *>());
                    auto handler2 = sinon::spy(sinon::detail::BlankHolder<void *>());
                    auto handler3 = sinon::spy(sinon::detail::BlankHolder<void *>());

                    promise.then(
                        null(),
                        [=](void *) -> void *
                        {
                            ((upromise::Promise::CallbackFn)handler1)(nullptr);
                            promise.then(null(), (Callback)handler3);
                            return nullptr;
                        });
                    promise.then(null(), (Callback)handler2);

                    promise.then(
                        null(),
                        [=](void *) -> void *
                        {
                            // Give implementations a bit of extra time to flush their internal queue, if necessary.
                            setTimeout(
                                [=]()
                                {
                                    CHECK(sinon::asserts::callOrder(handler1, handler2, handler3));
                                    done();
                                    return nullptr;
                                },
                                15ms);
                            return nullptr;
                        });
                });
            }
        }
    }

    EPILOGUE;
}

TEST_CASE("2.2.7: `then` must return a promise: `promise2 = promise1.then(onFulfilled, onRejected)`", "[2.2.7]")
{
    PROLOGUE;

    SECTION("is a promise")
    {
        // C++ do not need type check.
    }

    SECTION("2.2.7.1: If either `onFulfilled` or `onRejected` returns a value `x`, run the Promise Resolution "
            "Procedure `[[Resolve]](promise2, x)`")
    {
        SECTION("see separate 3.3 tests") {}
    }

    SECTION("2.2.7.2: If either `onFulfilled` or `onRejected` throws an exception `e`, `promise2` must be rejected "
            "with `e` as the reason.")
    {
        // C++ do not need type check.
        static void *expectedReason = (void *)"expectedReason";
        SECTION("The reason is void*")
        {
            testFulfilled(dummy, promise1, {
                auto promise2 = promise1.then(
                    [=](void *) -> void *
                    {
                        throw upromise::Promise::Error{expectedReason};
                    });

                promise2.then(
                    null(),
                    [=](void *actualReason) -> void *
                    {
                        CHECK(actualReason == expectedReason);
                        done();
                        return nullptr;
                    });
            });
            testRejected(dummy, promise1, {
                auto promise2 = promise1.then(
                    null(),
                    [=](void *) -> void *
                    {
                        throw upromise::Promise::Error{expectedReason};
                    });

                promise2.then(
                    null(),
                    [=](void *actualReason) -> void *
                    {
                        CHECK(actualReason == expectedReason);
                        done();
                        return nullptr;
                    });
            });
        }
    }

    SECTION("2.2.7.3: If `onFulfilled` is not a function and `promise1` is fulfilled, `promise2` must be fulfilled "
            "with the same value.")
    {
        // C++ do not need type check.
        SECTION("`onFulfilled` is null()")
        {
            testFulfilled(sentinel, promise1, {
                auto promise2 = promise1.then(null());

                promise2.then(
                    [=](void *value) -> void *
                    {
                        CHECK(value == sentinel);
                        done();
                        return nullptr;
                    });
            });
        }
    }

    SECTION("2.2.7.4: If `onRejected` is not a function and `promise1` is rejected, `promise2` must be rejected "
            "with the same reason.")
    {
        // C++ do not need type check.
        SECTION("`onRejected` is null()")
        {
            testRejected(sentinel, promise1, {
                auto promise2 = promise1.then(null(), null());

                promise2.then(
                    null(),
                    [=](void *reason) -> void *
                    {
                        CHECK(reason == sentinel);
                        done();
                        return nullptr;
                    });
            });
        }
    }

    EPILOGUE;
}

TEST_CASE("2.3.1: If `promise` and `x` refer to the same object, reject `promise` with a `TypeError' as the reason.", "[2.3.1]")
{
    PROLOGUE;

    SECTION("via return from a fulfilled promise")
    {
        SPECIFY_BEGIN;
        upromise::Promise *promise = new upromise::Promise(adapter.resolved(dummy));
        *promise = promise->then(
            [=](void *) -> upromise::Promise
            {
                auto ret = *promise;
                delete promise;
                return ret;
            });

        promise->then(
            null(),
            [=](void *reason) -> void *
            {
                CHECK(reason == upromise_recurse_error);
                done();
                return nullptr;
            });
        SPECIFY_END;
    }

    SECTION("via return from a rejected promise")
    {
        SPECIFY_BEGIN;
        upromise::Promise *promise = new upromise::Promise(adapter.rejected(dummy));
        *promise = promise->then(
            null(),
            [=](void *) -> upromise::Promise
            {
                auto ret = *promise;
                delete promise;
                return ret;
            });

        promise->then(
            null(),
            [=](void *reason) -> void *
            {
                CHECK(reason == upromise_recurse_error);
                done();
                return nullptr;
            });
        SPECIFY_END;
    }

    EPILOGUE;
}

#define testPromiseResolution(xFactory, var, ...)        \
    SECTION("via return from a fulfilled promise")       \
    {                                                    \
        SPECIFY_BEGIN;                                   \
        auto promise = adapter.resolved(dummy).then(     \
            [=](void *) -> decltype(xFactory()) {        \
                return xFactory();                       \
            });                                          \
                                                         \
        [=](upromise::Promise var) __VA_ARGS__(promise); \
        SPECIFY_END;                                     \
    }                                                    \
                                                         \
    SECTION("via return from a rejected promise")        \
    {                                                    \
        SPECIFY_BEGIN;                                   \
        auto promise = adapter.rejected(dummy).then(     \
            null(),                                      \
            [=](void *) -> decltype(xFactory()) {        \
                return xFactory();                       \
            });                                          \
                                                         \
        [=](upromise::Promise var) __VA_ARGS__(promise); \
        SPECIFY_END;                                     \
    }

TEST_CASE("2.3.2: If `x` is a promise, adopt its state", "[2.3.2]")
{
    PROLOGUE;

    SECTION("2.3.2.1: If `x` is pending, `promise` must remain pending until `x` is fulfilled or rejected.")
    {
        auto xFactory = [=]()
        {
            return adapter.deferred().promise;
        };

        testPromiseResolution(xFactory, promise, {
            auto wasFulfilled = Bool(false);
            auto wasRejected = Bool(false);

            promise.then(
                [=](void *) -> void *
                {
                    *wasFulfilled = true;
                    return nullptr;
                },
                [=](void *) -> void *
                {
                    *wasRejected = true;
                    return nullptr;
                });

            setTimeout(
                [=]()
                {
                    CHECK(*wasFulfilled == false);
                    CHECK(*wasRejected == false);
                    done();
                },
                100ms);
        });
    }

    SECTION("2.3.2.2: If/when `x` is fulfilled, fulfill `promise` with the same value.")
    {
        SECTION("`x` is already-fulfilled")
        {
            auto xFactory = [=]()
            {
                return adapter.resolved(sentinel);
            };

            testPromiseResolution(xFactory, promise, {
                promise.then(
                    [=](void *value) -> void *
                    {
                        CHECK(value == sentinel);
                        done();
                        return nullptr;
                    });
            });
        }

        SECTION("`x` is eventually-fulfilled")
        {
            auto d = std::make_shared<std::shared_ptr<Adapter::Defer>>(nullptr);

            auto xFactory = [=]()
            {
                *d = std::make_shared<Adapter::Defer>(adapter.deferred());
                setTimeout(
                    [=]()
                    {
                        (*d)->resolve(sentinel);
                    },
                    50ms);
                return (*d)->promise;
            };

            testPromiseResolution(xFactory, promise, {
                promise.then(
                    [=](void *value) -> void *
                    {
                        CHECK(value == sentinel);
                        done();
                        return nullptr;
                    });
            });
        }
    }

    SECTION("2.3.2.3: If/when `x` is rejected, reject `promise` with the same reason.")
    {
        SECTION("`x` is already-rejected")
        {
            auto xFactory = [=]()
            {
                return adapter.rejected(sentinel);
            };

            testPromiseResolution(xFactory, promise, {
                promise.then(
                    null(),
                    [=](void *reason) -> void *
                    {
                        CHECK(reason == sentinel);
                        done();
                        return nullptr;
                    });
            });
        }

        SECTION("`x` is eventually-rejected")
        {
            auto d = std::make_shared<std::shared_ptr<Adapter::Defer>>(nullptr);

            auto xFactory = [=]()
            {
                *d = std::make_shared<Adapter::Defer>(adapter.deferred());
                setTimeout(
                    [=]()
                    {
                        (*d)->reject(sentinel);
                    },
                    50ms);
                return (*d)->promise;
            };

            testPromiseResolution(xFactory, promise, {
                promise.then(
                    null(),
                    [=](void *reason) -> void *
                    {
                        CHECK(reason == sentinel);
                        done();
                        return nullptr;
                    });
            });
        }
    }

    EPILOGUE;
}

#define testCallingResolvePromise(yFactory, stringRepresentation, var, ...)                                 \
    DYNAMIC_SECTION("`y` is " << stringRepresentation)                                                      \
    {                                                                                                       \
        SECTION("`then` calls `resolvePromise` synchronously")                                              \
        {                                                                                                   \
            auto xFactory = [=]() {                                                                         \
                return std::make_shared<upromise::Thenable>(                                                \
                    [=](upromise::Thenable::ResolveNotifyFn resolvePromise, upromise::Thenable::NotifyFn) { \
                        resolvePromise(yFactory());                                                         \
                    });                                                                                     \
            };                                                                                              \
                                                                                                            \
            testPromiseResolution(xFactory, var, __VA_ARGS__);                                              \
        }                                                                                                   \
                                                                                                            \
        SECTION("`then` calls `resolvePromise` asynchronously")                                             \
        {                                                                                                   \
            auto xFactory = [=]() {                                                                         \
                return std::make_shared<upromise::Thenable>(                                                \
                    [=](upromise::Thenable::ResolveNotifyFn resolvePromise, upromise::Thenable::NotifyFn) { \
                        setTimeout([=]() { resolvePromise(yFactory()); }, 0ms);                             \
                    });                                                                                     \
            };                                                                                              \
                                                                                                            \
            testPromiseResolution(xFactory, var, __VA_ARGS__);                                              \
        }                                                                                                   \
    }

#define testCallingResolvePromiseFulfillsWith(yFactory, stringRepresentation, fulfillmentValue) \
    testCallingResolvePromise(yFactory, stringRepresentation, promise, {                        \
        promise.then(                                                                           \
            [=](void *value) -> void * {                                                        \
                CHECK(value == fulfillmentValue);                                               \
                done();                                                                         \
                return nullptr;                                                                 \
            });                                                                                 \
    });

#define testCallingResolvePromiseRejectsWith(yFactory, stringRepresentation, rejectionReason) \
    testCallingResolvePromise(yFactory, stringRepresentation, promise, {                      \
        promise.then(                                                                         \
            null(),                                                                           \
            [=](void *reason) -> void * {                                                     \
                CHECK(reason == rejectionReason);                                             \
                done();                                                                       \
                return nullptr;                                                               \
            });                                                                               \
    });

// `Thenable` is C++ only
TEST_CASE("2.3.3: Otherwise, if `x` is an object or function,", "[2.3.3]")
{
    PROLOGUE;
    auto thenables = helper::thenables(adapter, setTimeout);

    SECTION("2.3.3.1: Let `then` be `x.then`")
    {
        // C++ do not need a pre get.
    }

    SECTION("2.3.3.2: If retrieving the property `x.then` results in a thrown exception `e`, reject `promise` with "
            "`e` as the reason.")
    {
        // C++ can not throw any error here.
    }

    SECTION("2.3.3.3: If `then` is a function, call it with `x` as `this`, first argument `resolvePromise`, and "
            "second argument `rejectPromise`")
    {
        SECTION("Calls with `x` as `this` and two function arguments")
        {
            auto xFactory = [=]() -> upromise::Thenable::Ptr
            {
                auto x = std::make_shared<upromise::Thenable::Ptr>();
                class T : public upromise::Thenable
                {
                    std::shared_ptr<upromise::Thenable::Ptr> x;

                public:
                    T(std::shared_ptr<upromise::Thenable::Ptr> x) : x(x) {}
                    virtual void then(ResolveNotifyFn onFulfilled, NotifyFn onRejected) override
                    {
                        CHECK(this == (*x).get());
                        CHECK(onFulfilled != nullptr);
                        CHECK(onRejected != nullptr);
                        onFulfilled(nullptr);
                    }
                };
                *x = (upromise::Thenable::Ptr)std::make_shared<T>(x);
                return *x;
            };

            testPromiseResolution(xFactory, promise, {
                promise.then(
                    [=](void *) -> void *
                    {
                        done();
                        return nullptr;
                    });
            });
        }

        SECTION("Uses the original value of `then`")
        {
            // C++ is typed language, `Thenable` must has the correct `then` type.
        }

        SECTION("2.3.3.3.1: If/when `resolvePromise` is called with value `y`, run `[[Resolve]](promise, y)`")
        {
            SECTION("`y` is not a thenable")
            {
                auto factory = [=]()
                {
                    return dummy;
                };
                testCallingResolvePromiseFulfillsWith(factory, "void*", dummy);
            }

            SECTION("`y` is a thenable")
            {
                for (const auto &it : thenables.fulfilled)
                {
                    auto yFactory = [=]()
                    {
                        return it.second(sentinel);
                    };

                    testCallingResolvePromiseFulfillsWith(yFactory, it.first, sentinel);
                }

                for (const auto &it : thenables.rejected)
                {
                    auto yFactory = [=]()
                    {
                        return it.second(sentinel);
                    };

                    testCallingResolvePromiseRejectsWith(yFactory, it.first, sentinel);
                }
            }

            SECTION("`y` is a thenable for a thenable")
            {
                for (const auto &outerI : thenables.fulfilled)
                {
                    auto outerThenableFactory = outerI.second;

                    for (const auto &innerI : thenables.fulfilled)
                    {
                        auto innerThenableFactory = innerI.second;

                        std::string stringRepresentation = outerI.first + " for " + innerI.first;

                        auto yFactory = [=]()
                        {
                            return outerThenableFactory(innerThenableFactory(sentinel));
                        };

                        testCallingResolvePromiseFulfillsWith(yFactory, stringRepresentation, sentinel);
                    }

                    for (const auto &innerI : thenables.rejected)
                    {
                        auto innerThenableFactory = innerI.second;

                        std::string stringRepresentation = outerI.first + " for " + innerI.first;

                        auto yFactory = [=]()
                        {
                            return outerThenableFactory(innerThenableFactory(sentinel));
                        };

                        testCallingResolvePromiseRejectsWith(yFactory, stringRepresentation, sentinel);
                    }
                }
            }
        }

        SECTION("2.3.3.3.2: If/when `rejectPromise` is called with reason `r`, reject `promise` with `r`")
        {
            // only `void*` can be rejected
        }

        SECTION("2.3.3.3.3: If both `resolvePromise` and `rejectPromise` are called, or multiple calls to the same "
                "argument are made, the first call takes precedence, and any further calls are ignored.")
        {
            SECTION("calling `resolvePromise` then `rejectPromise`, both synchronously")
            {
                auto xFactory = [=]()
                {
                    return std::make_shared<upromise::Thenable>(
                        [=](upromise::Thenable::ResolveNotifyFn resolvePromise, upromise::Thenable::NotifyFn rejectPromise)
                        {
                            resolvePromise(sentinel);
                            rejectPromise(other);
                        });
                };

                testPromiseResolution(xFactory, promise, {
                    promise.then(
                        [=](void *value)
                        {
                            CHECK(value == sentinel);
                            done();
                            return nullptr;
                        });
                });
            }

            SECTION("calling `resolvePromise` synchronously then `rejectPromise` asynchronously")
            {
                auto xFactory = [=]()
                {
                    return std::make_shared<upromise::Thenable>(
                        [=](upromise::Thenable::ResolveNotifyFn resolvePromise, upromise::Thenable::NotifyFn rejectPromise)
                        {
                            resolvePromise(sentinel);

                            setTimeout(
                                [=]()
                                {
                                    rejectPromise(other);
                                },
                                0ms);
                        });
                };

                testPromiseResolution(xFactory, promise, {
                    promise.then(
                        [=](void *value)
                        {
                            CHECK(value == sentinel);
                            done();
                            return nullptr;
                        });
                });
            }

            SECTION("calling `resolvePromise` then `rejectPromise`, both asynchronously")
            {
                auto xFactory = [=]()
                {
                    return std::make_shared<upromise::Thenable>(
                        [=](upromise::Thenable::ResolveNotifyFn resolvePromise, upromise::Thenable::NotifyFn rejectPromise)
                        {
                            setTimeout(
                                [=]()
                                {
                                    resolvePromise(sentinel);
                                },
                                0ms);

                            setTimeout(
                                [=]()
                                {
                                    rejectPromise(other);
                                },
                                0ms);
                        });
                };

                testPromiseResolution(xFactory, promise, {
                    promise.then(
                        [=](void *value)
                        {
                            CHECK(value == sentinel);
                            done();
                            return nullptr;
                        });
                });
            }

            SECTION("calling `resolvePromise` with an asynchronously-fulfilled promise, then calling "
                    "`rejectPromise`, both synchronously")
            {
                auto xFactory = [=]()
                {
                    auto d = adapter.deferred();
                    setTimeout(
                        [=]()
                        {
                            d.resolve(sentinel);
                        },
                        50ms);

                    return std::make_shared<upromise::Thenable>(
                        [=](upromise::Thenable::ResolveNotifyFn resolvePromise, upromise::Thenable::NotifyFn rejectPromise)
                        {
                            resolvePromise(d.promise);
                            rejectPromise(other);
                        });
                };

                testPromiseResolution(xFactory, promise, {
                    promise.then(
                        [=](void *value)
                        {
                            CHECK(value == sentinel);
                            done();
                            return nullptr;
                        });
                });
            }

            SECTION("calling `resolvePromise` with an asynchronously-rejected promise, then calling "
                    "`rejectPromise`, both synchronously")
            {
                auto xFactory = [=]()
                {
                    auto d = adapter.deferred();
                    setTimeout(
                        [=]()
                        {
                            d.reject(sentinel);
                        },
                        50ms);

                    return std::make_shared<upromise::Thenable>(
                        [=](upromise::Thenable::ResolveNotifyFn resolvePromise, upromise::Thenable::NotifyFn rejectPromise)
                        {
                            resolvePromise(d.promise);
                            rejectPromise(other);
                        });
                };

                testPromiseResolution(xFactory, promise, {
                    promise.then(
                        null(),
                        [=](void *reason)
                        {
                            CHECK(reason == sentinel);
                            done();
                            return nullptr;
                        });
                });
            }

            SECTION("calling `rejectPromise` then `resolvePromise`, both synchronously")
            {
                auto xFactory = [=]()
                {
                    return std::make_shared<upromise::Thenable>(
                        [=](upromise::Thenable::ResolveNotifyFn resolvePromise, upromise::Thenable::NotifyFn rejectPromise)
                        {
                            rejectPromise(sentinel);
                            resolvePromise(other);
                        });
                };

                testPromiseResolution(xFactory, promise, {
                    promise.then(
                        null(),
                        [=](void *reason)
                        {
                            CHECK(reason == sentinel);
                            done();
                            return nullptr;
                        });
                });
            }

            SECTION("calling `rejectPromise` synchronously then `resolvePromise` asynchronously")
            {
                auto xFactory = [=]()
                {
                    return std::make_shared<upromise::Thenable>(
                        [=](upromise::Thenable::ResolveNotifyFn resolvePromise, upromise::Thenable::NotifyFn rejectPromise)
                        {
                            rejectPromise(sentinel);

                            setTimeout(
                                [=]()
                                {
                                    resolvePromise(other);
                                },
                                0ms);
                        });
                };

                testPromiseResolution(xFactory, promise, {
                    promise.then(
                        null(),
                        [=](void *reason)
                        {
                            CHECK(reason == sentinel);
                            done();
                            return nullptr;
                        });
                });
            }

            SECTION("calling `rejectPromise` then `resolvePromise`, both asynchronously")
            {
                auto xFactory = [=]()
                {
                    return std::make_shared<upromise::Thenable>(
                        [=](upromise::Thenable::ResolveNotifyFn resolvePromise, upromise::Thenable::NotifyFn rejectPromise)
                        {
                            setTimeout(
                                [=]()
                                {
                                    rejectPromise(sentinel);
                                },
                                0ms);

                            setTimeout(
                                [=]()
                                {
                                    resolvePromise(other);
                                },
                                0ms);
                        });
                };

                testPromiseResolution(xFactory, promise, {
                    promise.then(
                        null(),
                        [=](void *reason)
                        {
                            CHECK(reason == sentinel);
                            done();
                            return nullptr;
                        });
                });
            }

            SECTION("calling `resolvePromise` twice synchronously")
            {
                auto xFactory = [=]()
                {
                    return std::make_shared<upromise::Thenable>(
                        [=](upromise::Thenable::ResolveNotifyFn resolvePromise, upromise::Thenable::NotifyFn)
                        {
                            resolvePromise(sentinel);
                            resolvePromise(other);
                        });
                };

                testPromiseResolution(xFactory, promise, {
                    promise.then(
                        [=](void *value)
                        {
                            CHECK(value == sentinel);
                            done();
                            return nullptr;
                        });
                });
            }

            SECTION("calling `resolvePromise` twice, first synchronously then asynchronously")
            {
                auto xFactory = [=]()
                {
                    return std::make_shared<upromise::Thenable>(
                        [=](upromise::Thenable::ResolveNotifyFn resolvePromise, upromise::Thenable::NotifyFn)
                        {
                            resolvePromise(sentinel);

                            setTimeout(
                                [=]()
                                {
                                    resolvePromise(other);
                                },
                                0ms);
                        });
                };

                testPromiseResolution(xFactory, promise, {
                    promise.then(
                        [=](void *value)
                        {
                            CHECK(value == sentinel);
                            done();
                            return nullptr;
                        });
                });
            }

            SECTION("calling `resolvePromise` twice, both times asynchronously")
            {
                auto xFactory = [=]()
                {
                    return std::make_shared<upromise::Thenable>(
                        [=](upromise::Thenable::ResolveNotifyFn resolvePromise, upromise::Thenable::NotifyFn)
                        {
                            setTimeout(
                                [=]()
                                {
                                    resolvePromise(sentinel);
                                },
                                0ms);

                            setTimeout(
                                [=]()
                                {
                                    resolvePromise(other);
                                },
                                0ms);
                        });
                };

                testPromiseResolution(xFactory, promise, {
                    promise.then(
                        [=](void *value)
                        {
                            CHECK(value == sentinel);
                            done();
                            return nullptr;
                        });
                });
            }

            SECTION("calling `resolvePromise` with an asynchronously-fulfilled promise, then calling it again, both "
                    "times synchronously")
            {
                auto xFactory = [=]()
                {
                    auto d = adapter.deferred();
                    setTimeout(
                        [=]()
                        {
                            d.resolve(sentinel);
                        },
                        50ms);

                    return std::make_shared<upromise::Thenable>(
                        [=](upromise::Thenable::ResolveNotifyFn resolvePromise, upromise::Thenable::NotifyFn)
                        {
                            resolvePromise(d.promise);
                            resolvePromise(other);
                        });
                };

                testPromiseResolution(xFactory, promise, {
                    promise.then(
                        [=](void *value)
                        {
                            CHECK(value == sentinel);
                            done();
                            return nullptr;
                        });
                });
            }

            SECTION("calling `resolvePromise` with an asynchronously-rejected promise, then calling it again, both "
                    "times synchronously")
            {
                auto xFactory = [=]()
                {
                    auto d = adapter.deferred();
                    setTimeout(
                        [=]()
                        {
                            d.reject(sentinel);
                        },
                        50ms);

                    return std::make_shared<upromise::Thenable>(
                        [=](upromise::Thenable::ResolveNotifyFn resolvePromise, upromise::Thenable::NotifyFn)
                        {
                            resolvePromise(d.promise);
                            resolvePromise(other);
                        });
                };

                testPromiseResolution(xFactory, promise, {
                    promise.then(
                        null(),
                        [=](void *reason)
                        {
                            CHECK(reason == sentinel);
                            done();
                            return nullptr;
                        });
                });
            }

            SECTION("calling `rejectPromise` twice synchronously")
            {
                auto xFactory = [=]()
                {
                    return std::make_shared<upromise::Thenable>(
                        [=](upromise::Thenable::ResolveNotifyFn resolvePromise, upromise::Thenable::NotifyFn rejectPromise)
                        {
                            rejectPromise(sentinel);
                            rejectPromise(other);
                        });
                };

                testPromiseResolution(xFactory, promise, {
                    promise.then(
                        null(),
                        [=](void *reason)
                        {
                            CHECK(reason == sentinel);
                            done();
                            return nullptr;
                        });
                });
            }

            SECTION("calling `rejectPromise` twice, first synchronously then asynchronously")
            {
                auto xFactory = [=]()
                {
                    return std::make_shared<upromise::Thenable>(
                        [=](upromise::Thenable::ResolveNotifyFn resolvePromise, upromise::Thenable::NotifyFn rejectPromise)
                        {
                            rejectPromise(sentinel);

                            setTimeout(
                                [=]()
                                {
                                    rejectPromise(other);
                                },
                                0ms);
                        });
                };

                testPromiseResolution(xFactory, promise, {
                    promise.then(
                        null(),
                        [=](void *reason)
                        {
                            CHECK(reason == sentinel);
                            done();
                            return nullptr;
                        });
                });
            }

            SECTION("calling `rejectPromise` twice, both times asynchronously")
            {
                auto xFactory = [=]()
                {
                    return std::make_shared<upromise::Thenable>(
                        [=](upromise::Thenable::ResolveNotifyFn resolvePromise, upromise::Thenable::NotifyFn rejectPromise)
                        {
                            setTimeout(
                                [=]()
                                {
                                    rejectPromise(sentinel);
                                },
                                0ms);

                            setTimeout(
                                [=]()
                                {
                                    rejectPromise(other);
                                },
                                0ms);
                        });
                };

                testPromiseResolution(xFactory, promise, {
                    promise.then(
                        null(),
                        [=](void *reason)
                        {
                            CHECK(reason == sentinel);
                            done();
                            return nullptr;
                        });
                });
            }

            SECTION("saving and abusing `resolvePromise` and `rejectPromise`")
            {
                auto savedResolvePromise = std::make_shared<upromise::Thenable::ResolveNotifyFn>();
                auto savedRejectPromise = std::make_shared<upromise::Thenable::NotifyFn>();

                auto xFactory = [=]()
                {
                    return std::make_shared<upromise::Thenable>(
                        [=](upromise::Thenable::ResolveNotifyFn resolvePromise, upromise::Thenable::NotifyFn rejectPromise)
                        {
                            *savedResolvePromise = resolvePromise;
                            *savedRejectPromise = rejectPromise;
                        });
                };

                testPromiseResolution(xFactory, promise, {
                    auto timesFulfilled = Int(0);
                    auto timesRejected = Int(0);

                    promise.then(
                        [=](void *) -> void *
                        {
                            ++*timesFulfilled;
                            return nullptr;
                        },
                        [=](void *) -> void *
                        {
                            ++*timesRejected;
                            return nullptr;
                        });

                    if (*savedResolvePromise && *savedRejectPromise)
                    {
                        (*savedResolvePromise)(dummy);
                        (*savedResolvePromise)(dummy);
                        (*savedRejectPromise)(dummy);
                        (*savedRejectPromise)(dummy);
                    }

                    setTimeout(
                        [=]()
                        {
                            (*savedResolvePromise)(dummy);
                            (*savedResolvePromise)(dummy);
                            (*savedRejectPromise)(dummy);
                            (*savedRejectPromise)(dummy);
                        },
                        50ms);

                    setTimeout(
                        [=]()
                        {
                            CHECK(*timesFulfilled == 1);
                            CHECK(*timesRejected == 0);
                            done();
                            return nullptr;
                        },
                        100ms);
                });
            }
        }

        SECTION("2.3.3.3.4: If calling `then` throws an exception `e`,")
        {
            SECTION("2.3.3.3.4.1: If `resolvePromise` or `rejectPromise` have been called, ignore it.")
            {
                SECTION("`resolvePromise` was called with a non-thenable")
                {
                    auto xFactory = [=]()
                    {
                        return std::make_shared<upromise::Thenable>(
                            [=](upromise::Thenable::ResolveNotifyFn resolvePromise, upromise::Thenable::NotifyFn)
                            {
                                resolvePromise(sentinel);
                                throw upromise::Promise::Error{other};
                            });
                    };

                    testPromiseResolution(xFactory, promise, {
                        promise.then(
                            [=](void *value)
                            {
                                CHECK(value == sentinel);
                                done();
                                return nullptr;
                            });
                    });
                }

                SECTION("`resolvePromise` was called with an asynchronously-fulfilled promise")
                {
                    auto xFactory = [=]()
                    {
                        auto d = adapter.deferred();
                        setTimeout(
                            [=]()
                            {
                                d.resolve(sentinel);
                            },
                            50ms);

                        return std::make_shared<upromise::Thenable>(
                            [=](upromise::Thenable::ResolveNotifyFn resolvePromise, upromise::Thenable::NotifyFn)
                            {
                                resolvePromise(d.promise);
                                throw upromise::Promise::Error{other};
                            });
                    };

                    testPromiseResolution(xFactory, promise, {
                        promise.then(
                            [=](void *value)
                            {
                                CHECK(value == sentinel);
                                done();
                                return nullptr;
                            });
                    });
                }

                SECTION("`resolvePromise` was called with an asynchronously-rejected promise")
                {
                    auto xFactory = [=]()
                    {
                        auto d = adapter.deferred();
                        setTimeout(
                            [=]()
                            {
                                d.reject(sentinel);
                            },
                            50ms);

                        return std::make_shared<upromise::Thenable>(
                            [=](upromise::Thenable::ResolveNotifyFn resolvePromise, upromise::Thenable::NotifyFn)
                            {
                                resolvePromise(d.promise);
                                throw upromise::Promise::Error{other};
                            });
                    };

                    testPromiseResolution(xFactory, promise, {
                        promise.then(
                            null(),
                            [=](void *reason)
                            {
                                CHECK(reason == sentinel);
                                done();
                                return nullptr;
                            });
                    });
                }

                SECTION("`rejectPromise` was called")
                {
                    auto xFactory = [=]()
                    {
                        return std::make_shared<upromise::Thenable>(
                            [=](upromise::Thenable::ResolveNotifyFn resolvePromise, upromise::Thenable::NotifyFn rejectPromise)
                            {
                                rejectPromise(sentinel);
                                throw upromise::Promise::Error{other};
                            });
                    };

                    testPromiseResolution(xFactory, promise, {
                        promise.then(
                            null(),
                            [=](void *reason)
                            {
                                CHECK(reason == sentinel);
                                done();
                                return nullptr;
                            });
                    });
                }

                SECTION("`resolvePromise` then `rejectPromise` were called")
                {
                    auto xFactory = [=]()
                    {
                        return std::make_shared<upromise::Thenable>(
                            [=](upromise::Thenable::ResolveNotifyFn resolvePromise, upromise::Thenable::NotifyFn rejectPromise)
                            {
                                resolvePromise(sentinel);
                                rejectPromise(other);
                                throw upromise::Promise::Error{other};
                            });
                    };

                    testPromiseResolution(xFactory, promise, {
                        promise.then(
                            [=](void *value)
                            {
                                CHECK(value == sentinel);
                                done();
                                return nullptr;
                            });
                    });
                }

                SECTION("`rejectPromise` then `resolvePromise` were called")
                {
                    auto xFactory = [=]()
                    {
                        return std::make_shared<upromise::Thenable>(
                            [=](upromise::Thenable::ResolveNotifyFn resolvePromise, upromise::Thenable::NotifyFn rejectPromise)
                            {
                                rejectPromise(sentinel);
                                resolvePromise(other);
                                throw upromise::Promise::Error{other};
                            });
                    };

                    testPromiseResolution(xFactory, promise, {
                        promise.then(
                            null(),
                            [=](void *reason)
                            {
                                CHECK(reason == sentinel);
                                done();
                                return nullptr;
                            });
                    });
                }
            }

            SECTION("2.3.3.3.4.2: Otherwise, reject `promise` with `e` as the reason.")
            {
                SECTION("straightforward case")
                {
                    auto xFactory = [=]()
                    {
                        return std::make_shared<upromise::Thenable>(
                            [=](upromise::Thenable::ResolveNotifyFn, upromise::Thenable::NotifyFn)
                            {
                                throw upromise::Promise::Error{sentinel};
                            });
                    };

                    testPromiseResolution(xFactory, promise, {
                        promise.then(
                            null(),
                            [=](void *reason)
                            {
                                CHECK(reason == sentinel);
                                done();
                                return nullptr;
                            });
                    });
                }

                SECTION("`resolvePromise` is called asynchronously before the `throw`")
                {
                    auto xFactory = [=]()
                    {
                        return std::make_shared<upromise::Thenable>(
                            [=](upromise::Thenable::ResolveNotifyFn resolvePromise, upromise::Thenable::NotifyFn)
                            {
                                setTimeout(
                                    [=]()
                                    {
                                        resolvePromise(other);
                                    },
                                    0ms);
                                throw upromise::Promise::Error{sentinel};
                            });
                    };

                    testPromiseResolution(xFactory, promise, {
                        promise.then(
                            null(),
                            [=](void *reason)
                            {
                                CHECK(reason == sentinel);
                                done();
                                return nullptr;
                            });
                    });
                }

                SECTION("`rejectPromise` is called asynchronously before the `throw`")
                {
                    auto xFactory = [=]()
                    {
                        return std::make_shared<upromise::Thenable>(
                            [=](upromise::Thenable::ResolveNotifyFn resolvePromise, upromise::Thenable::NotifyFn rejectPromise)
                            {
                                setTimeout(
                                    [=]()
                                    {
                                        rejectPromise(other);
                                    },
                                    0ms);
                                throw upromise::Promise::Error{sentinel};
                            });
                    };

                    testPromiseResolution(xFactory, promise, {
                        promise.then(
                            null(),
                            [=](void *reason)
                            {
                                CHECK(reason == sentinel);
                                done();
                                return nullptr;
                            });
                    });
                }
            }
        }
    }

    SECTION("2.3.3.4: If `then` is not a function, fulfill promise with `x`")
    {
        // C++ do not need type check.
    }

    EPILOGUE;
}

TEST_CASE("2.3.4: If `x` is not an object or function, fulfill `promise` with `x`", "[2.3.4]")
{
    // Typed language do not need type test.
}
