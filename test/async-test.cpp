#include <catch2/catch.hpp>
#include <upromise/async.h>
#include "test.hpp"

extern void *dummy;
extern void *sentinel;

TEST_CASE("async/await demo", "[async]")
{
    PROLOGUE;

    SECTION("order")
    {
        SPECIFY_BEGIN;

        adapter.resolved(dummy).then(
            [&](void *) -> void *
            {
                auto x = Int(0);

                auto F = upromise::async(
                    event_loop.dispatcher,
                    [=](upromise::AsyncContext ctx) -> void *
                    {
                        CHECK(*x == 1);
                        *x += 1;
                        return nullptr;
                    });

                auto fn = upromise::async(
                    event_loop.dispatcher,
                    [=](upromise::AsyncContext ctx) -> void *
                    {
                        CHECK(*x == 0);
                        *x += 1;
                        ctx.await(F());
                        CHECK(*x == 2);
                        *x += 1;
                        return nullptr;
                    });

                CHECK(*x == 0);
                fn();
                CHECK(*x == 2);

                adapter.resolved(dummy).then(
                    [=](void *) -> void *
                    {
                        CHECK(*x == 3);
                        *x += 1;
                        return nullptr;
                    });

                done();
                return nullptr;
            });

        SPECIFY_END;
    }

    SECTION("throw")
    {
        SPECIFY_BEGIN;

        adapter.resolved(dummy).then(
            [&](void *) -> void *
            {
                auto x = Int(0);

                auto fn = upromise::async(
                    event_loop.dispatcher,
                    [=](upromise::AsyncContext ctx) -> void *
                    {
                        throw upromise::Error{sentinel};
                        return dummy;
                    });

                fn().then(
                    null(),
                    [=](void *data) -> void *
                    {
                        CHECK(data == sentinel);
                        done();
                        return nullptr;
                    });

                return nullptr;
            });

        SPECIFY_END;
    }

    SECTION("await rejected")
    {
        SPECIFY_BEGIN;

        adapter.resolved(dummy).then(
            [&](void *) -> void *
            {
                auto x = Int(0);

                auto fn = upromise::async(
                    event_loop.dispatcher,
                    [=](upromise::AsyncContext ctx) -> void *
                    {
                        ctx.await(adapter.rejected(sentinel));
                        return dummy;
                    });

                fn().then(
                    null(),
                    [=](void *data) -> void *
                    {
                        CHECK(data == sentinel);
                        done();
                        return nullptr;
                    });

                return nullptr;
            });

        SPECIFY_END;
    }

    EPILOGUE;
}

extern void *sentinel2;
extern void *sentinel3;

TEST_CASE("generator demo", "[async]")
{
    PROLOGUE;

    SECTION("next()")
    {
        SPECIFY_BEGIN;

        adapter.resolved(dummy).then(
            [&](void *) -> void *
            {
                auto Fn = upromise::generator(
                    event_loop.dispatcher,
                    [=](upromise::Generator *gen) -> void *
                    {
                        void *receive;
                        Yield(receive, gen, sentinel);
                        CHECK(receive == nullptr);
                        Yield(receive, gen, sentinel2);
                        CHECK(receive == sentinel3);
                        return dummy;
                    });

                auto gen = Fn();
                auto iter = gen.next();
                CHECK(iter.done == false);
                CHECK(iter.data == sentinel);
                iter = gen.next(sentinel3);
                CHECK(iter.done == false);
                CHECK(iter.data == sentinel2);
                iter = gen.next();
                CHECK(iter.done == true);
                CHECK(iter.data == dummy);
                iter = gen.next();
                CHECK(iter.done == true);
                CHECK(iter.data == nullptr);

                done();
                return nullptr;
            });

        SPECIFY_END;
    }

    SECTION("throw in generator")
    {
        SPECIFY_BEGIN;

        adapter.resolved(dummy).then(
            [&](void *) -> void *
            {
                auto Fn = upromise::generator(
                    event_loop.dispatcher,
                    [=](upromise::Generator *gen) -> void *
                    {
                        throw upromise::Error{sentinel};
                        return dummy;
                    });

                auto gen = Fn();
                try
                {
                    gen.next();
                }
                catch (upromise::Error err)
                {
                    CHECK(err.err == sentinel);

                    auto iter = gen.next();
                    CHECK(iter.done == true);
                    CHECK(iter.data == nullptr);

                    done();
                }

                return nullptr;
            });

        SPECIFY_END;
    }

    SECTION("return()")
    {
        SPECIFY_BEGIN;

        adapter.resolved(dummy).then(
            [&](void *) -> void *
            {
                auto Fn = upromise::generator(
                    event_loop.dispatcher,
                    [=](upromise::Generator *gen) -> void *
                    {
                        void *receive;
                        Yield(receive, gen, sentinel);
                        CHECK(false);
                        Yield(receive, gen, sentinel2);
                        return dummy;
                    });

                auto gen = Fn();
                auto iter = gen.next();
                CHECK(iter.done == false);
                CHECK(iter.data == sentinel);
                iter = gen.Return(sentinel3);
                CHECK(iter.done == true);
                CHECK(iter.data == sentinel3);
                iter = gen.next();
                CHECK(iter.done == true);
                CHECK(iter.data == nullptr);

                done();
                return nullptr;
            });

        SPECIFY_END;
    }

    SECTION("throw()")
    {
        SPECIFY_BEGIN;

        adapter.resolved(dummy).then(
            [&](void *) -> void *
            {
                auto Fn = upromise::generator(
                    event_loop.dispatcher,
                    [=](upromise::Generator *gen) -> void *
                    {
                        void *receive;
                        Yield(receive, gen, sentinel);
                        CHECK(false);
                        Yield(receive, gen, sentinel2);
                        return dummy;
                    });

                auto gen = Fn();
                auto iter = gen.next();
                CHECK(iter.done == false);
                CHECK(iter.data == sentinel);
                try
                {
                    gen.Throw(sentinel3);
                }
                catch (upromise::Error err)
                {
                    CHECK(err.err == sentinel3);

                    iter = gen.next();
                    CHECK(iter.done == true);
                    CHECK(iter.data == nullptr);

                    done();
                }

                return nullptr;
            });

        SPECIFY_END;
    }

    EPILOGUE;
}

TEST_CASE("async-generator demo", "[async]")
{
    PROLOGUE;

    SECTION("order")
    {
        SPECIFY_BEGIN;

        adapter.resolved(dummy).then(
            [&](void *) -> void *
            {
                auto x = Int(0);
                auto Fn = upromise::agen(
                    event_loop.dispatcher,
                    [=](upromise::AsyncGenerator *gen) -> void *
                    {
                        void *receive;
                        CHECK(*x == 0);
                        *x += 1;
                        AYield(receive, gen, adapter.resolved(sentinel));
                        CHECK(*x == 2);
                        *x += 1;
                        AYield(receive, gen, adapter.resolved(sentinel2));
                        return dummy;
                    });

                auto gen = std::make_shared<upromise::AsyncGenerator>(Fn());
                CHECK(*x == 0);

                adapter.resolved(dummy).then(
                    [=](void *) -> void *
                    {
                        CHECK(*x == 1);
                        *x += 1;
                        return nullptr;
                    });

                gen->next()
                    .then(
                        [=](void *data_raw)
                        {
                            auto data = (upromise::Generator::Result *)data_raw;
                            CHECK(data->done == false);
                            CHECK(data->data == sentinel);
                            free(data);
                            return gen->next();
                        })
                    .then(
                        [=](void *data_raw)
                        {
                            auto data = (upromise::Generator::Result *)data_raw;
                            CHECK(data->done == false);
                            CHECK(data->data == sentinel2);
                            free(data);
                            return gen->next();
                        })
                    .then(
                        [=](void *data_raw)
                        {
                            auto data = (upromise::Generator::Result *)data_raw;
                            CHECK(data->done == true);
                            CHECK(data->data == dummy);
                            free(data);
                            return gen->next();
                        })
                    .then(
                        [=](void *data_raw)
                        {
                            auto data = (upromise::Generator::Result *)data_raw;
                            CHECK(data->done == true);
                            CHECK(data->data == nullptr);
                            free(data);
                            done();
                            return nullptr;
                        });

                return nullptr;
            });

        SPECIFY_END;
    }

    SECTION("sync next()")
    {
        SPECIFY_BEGIN;

        adapter.resolved(dummy).then(
            [&](void *) -> void *
            {
                auto x = Int(0);
                auto Fn = upromise::agen(
                    event_loop.dispatcher,
                    [=](upromise::AsyncGenerator *gen) -> void *
                    {
                        void *receive;
                        CHECK(*x == 0);
                        *x += 1;
                        AYield(receive, gen, adapter.resolved(sentinel));
                        CHECK(*x == 2);
                        *x += 1;
                        AYield(receive, gen, adapter.resolved(sentinel2));
                        CHECK(receive == sentinel3);
                        return dummy;
                    });

                auto gen = std::make_shared<upromise::AsyncGenerator>(Fn());
                CHECK(*x == 0);

                adapter.resolved(dummy).then(
                    [=](void *) -> void *
                    {
                        CHECK(*x == 1);
                        *x += 1;
                        return nullptr;
                    });

                gen->next().then(
                    [=](void *data_raw)
                    {
                        auto data = (upromise::Generator::Result *)data_raw;
                        CHECK(data->done == false);
                        CHECK(data->data == sentinel);
                        free(data);
                        return nullptr;
                    });
                gen->next(sentinel3).then(
                    [=](void *data_raw)
                    {
                        auto data = (upromise::Generator::Result *)data_raw;
                        CHECK(data->done == false);
                        CHECK(data->data == sentinel2);
                        free(data);
                        return nullptr;
                    });
                gen->next().then(
                    [=](void *data_raw)
                    {
                        auto data = (upromise::Generator::Result *)data_raw;
                        CHECK(data->done == true);
                        CHECK(data->data == dummy);
                        free(data);
                        return nullptr;
                    });
                gen->next().then(
                    [=](void *data_raw)
                    {
                        auto data = (upromise::Generator::Result *)data_raw;
                        CHECK(data->done == true);
                        CHECK(data->data == nullptr);
                        free(data);
                        done();
                        return nullptr;
                    });

                return nullptr;
            });

        SPECIFY_END;
    }

    SECTION("throw")
    {
        SPECIFY_BEGIN;

        adapter.resolved(dummy).then(
            [&](void *) -> void *
            {
                auto x = Int(0);
                auto Fn = upromise::agen(
                    event_loop.dispatcher,
                    [=](upromise::AsyncGenerator *gen) -> void *
                    {
                        throw upromise::Error{sentinel};
                        return dummy;
                    });

                auto gen = std::make_shared<upromise::AsyncGenerator>(Fn());

                gen->next().then(
                    null(),
                    [=](void *data)
                    {
                        CHECK(data == sentinel);
                        done();
                        return nullptr;
                    });

                return nullptr;
            });

        SPECIFY_END;
    }

    SECTION("yield rejected")
    {
        SPECIFY_BEGIN;

        adapter.resolved(dummy).then(
            [&](void *) -> void *
            {
                auto x = Int(0);
                auto Fn = upromise::agen(
                    event_loop.dispatcher,
                    [=](upromise::AsyncGenerator *gen) -> void *
                    {
                        void *receive;
                        AYield(receive, gen, adapter.rejected(sentinel));
                        return dummy;
                    });

                auto gen = std::make_shared<upromise::AsyncGenerator>(Fn());

                gen->next().then(
                    null(),
                    [=](void *data)
                    {
                        CHECK(data == sentinel);
                        done();
                        return nullptr;
                    });

                return nullptr;
            });

        SPECIFY_END;
    }

    SECTION("return()")
    {
        SPECIFY_BEGIN;

        adapter.resolved(dummy).then(
            [&](void *) -> void *
            {
                auto Fn = upromise::agen(
                    event_loop.dispatcher,
                    [=](upromise::AsyncGenerator *gen) -> void *
                    {
                        void *receive;
                        AYield(receive, gen, adapter.resolved(sentinel));
                        CHECK(false);
                        AYield(receive, gen, adapter.resolved(sentinel2));
                        return dummy;
                    });

                auto gen = Fn();

                gen.next().then(
                    [=](void *data_raw) -> void *
                    {
                        auto data = (upromise::Generator::Result *)data_raw;
                        CHECK(data->done == false);
                        CHECK(data->data == sentinel);
                        free(data);
                        return nullptr;
                    });
                gen.Return(sentinel3).then(
                    [=](void *data_raw) -> void *
                    {
                        auto data = (upromise::Generator::Result *)data_raw;
                        CHECK(data->done == true);
                        CHECK(data->data == sentinel3);
                        free(data);
                        return nullptr;
                    });
                gen.next().then(
                    [=](void *data_raw) -> void *
                    {
                        auto data = (upromise::Generator::Result *)data_raw;
                        CHECK(data->done == true);
                        CHECK(data->data == nullptr);
                        free(data);
                        done();
                        return nullptr;
                    });

                return nullptr;
            });

        SPECIFY_END;
    }

    SECTION("throw()")
    {
        SPECIFY_BEGIN;

        adapter.resolved(dummy).then(
            [&](void *) -> void *
            {
                auto Fn = upromise::agen(
                    event_loop.dispatcher,
                    [=](upromise::AsyncGenerator *gen) -> void *
                    {
                        void *receive;
                        AYield(receive, gen, adapter.resolved(sentinel));
                        CHECK(false);
                        AYield(receive, gen, adapter.resolved(sentinel2));
                        return dummy;
                    });

                auto gen = Fn();

                gen.next().then(
                    [=](void *data_raw) -> void *
                    {
                        auto data = (upromise::Generator::Result *)data_raw;
                        CHECK(data->done == false);
                        CHECK(data->data == sentinel);
                        free(data);
                        return nullptr;
                    });
                gen.Throw(sentinel3).then(
                    null(),
                    [=](void *data) -> void *
                    {
                        CHECK(data == sentinel3);
                        return nullptr;
                    });
                gen.next().then(
                    [=](void *data_raw) -> void *
                    {
                        auto data = (upromise::Generator::Result *)data_raw;
                        CHECK(data->done == true);
                        CHECK(data->data == nullptr);
                        free(data);
                        done();
                        return nullptr;
                    });

                return nullptr;
            });

        SPECIFY_END;
    }

    EPILOGUE;
}
