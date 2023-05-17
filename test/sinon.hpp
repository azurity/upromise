#ifndef _SINON_HPP_
#define _SINON_HPP_

#include <any>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <tuple>

namespace sinon
{
    namespace helper
    {
        namespace detail
        {

            template <int... Is>
            struct seq
            {
            };

            template <int N, int... Is>
            struct gen_seq : gen_seq<N - 1, N - 1, Is...>
            {
            };

            template <int... Is>
            struct gen_seq<0, Is...> : seq<Is...>
            {
            };

            template <typename R, typename T, typename F, int... Is>
            R for_each(T &&t, F f, seq<Is...>)
            {
                return R{f(std::get<Is>(t), Is)...};
            }
        }

        template <typename R, typename... Ts, typename F>
        R for_each_in_tuple(std::tuple<Ts...> const &t, F f)
        {
            return detail::for_each<R>(t, f, detail::gen_seq<sizeof...(Ts)>());
        }
    }

    namespace detail
    {
        struct SpyData
        {
            std::vector<std::any> callArgs = {};
            size_t callCount = 0;
            std::chrono::steady_clock::time_point lastCallTime = std::chrono::steady_clock::time_point::min();
        };

        template <typename func_holder>
        class Spy
        {
            struct Desc
            {
                Desc(const std::shared_ptr<func_holder> &holder) : holder(holder) {}
                SpyData data;
                std::shared_ptr<func_holder> holder;
            };
            std::shared_ptr<Desc> desc;

            template <typename R, typename... Args>
            struct Callable
            {
                Callable(std::shared_ptr<Desc> desc) : desc(desc) {}
                std::shared_ptr<Desc> desc;
                R operator()(Args... args)
                {
                    desc->data.callArgs = helper::for_each_in_tuple<std::vector<std::any>>(
                        std::tuple<Args...>(args...),
                        [](auto &&v, int) -> std::any
                        {
                            return std::any(v);
                        });
                    desc->data.callCount += 1;
                    desc->data.lastCallTime = std::chrono::steady_clock::now();

                    if (desc->holder)
                        return (*desc->holder).operator std::function<R(Args...)>()(args...);
                    else
                        return R();
                }
            };

        public:
            Spy(const std::shared_ptr<func_holder> &holder) : desc(std::make_shared<Desc>(holder)) {}

            template <typename R, typename... Args>
            operator std::function<R(Args...)>() const
            {
                return Callable<R, Args...>(desc);
            }

            const SpyData *data() const
            {
                return &desc->data;
            }
        };

        template <typename T>
        struct BlankHolder
        {
            template <typename R, typename... Args>
            operator std::function<R(Args...)>() const
            {
                return [](Args...) -> R
                {
                    return R();
                };
            }
        };

        struct ThrowBase
        {
            ThrowBase() {}
            virtual void operator()() = 0;
        };

        template <typename Err>
        struct Throw : public ThrowBase
        {
            Err e;
            Throw(Err e) : e(e) {}
            virtual void operator()() override
            {
                throw e;
            }
        };

        class Stub
        {
            struct Desc
            {
                Spy<BlankHolder<void *>> spy;
                std::shared_ptr<ThrowBase> err;
                std::any ret;
                Desc() : spy(nullptr) {}
            };
            std::shared_ptr<Desc> desc;

            template <typename R, typename... Args>
            struct Callable
            {
                Callable(std::shared_ptr<Desc> desc) : desc(desc) {}
                std::shared_ptr<Desc> desc;
                R operator()(Args... args)
                {
                    desc->spy.operator std::function<R(Args...)>()(args...);
                    if (desc->err)
                        (*desc->err)();
                    return std::any_cast<R>(desc->ret);
                }
            };

        public:
            Stub() : desc(std::make_shared<Desc>()) {}

            Stub &returns(std::any value)
            {
                desc->ret = value;
                return *this;
            }

            template <typename E>
            Stub &throws(E value)
            {
                desc->err = std::move(std::make_shared<Throw<E>>(value));
                return *this;
            }

            template <typename R, typename... Args>
            operator std::function<R(Args...)>() const
            {
                return Callable<R, Args...>(desc);
            }

            const SpyData *data() const
            {
                return desc->spy.data();
            }
        };
    }

    template <typename T>
    detail::Spy<detail::BlankHolder<T>> spy()
    {
        return detail::Spy<detail::BlankHolder<T>>(nullptr);
    }

    template <typename T>
    detail::Spy<T> spy(const T &fn)
    {
        return detail::Spy<T>(std::make_shared<T>(fn));
    }

    detail::Stub stub()
    {
        return detail::Stub();
    }

    namespace match
    {
        template <typename T>
        class same
        {
            T value;

        public:
            same(T value) : value(value) {}
            bool operator()(const std::any &v) const
            {
                return v.type() == typeid(T) && std::any_cast<T>(v) == value;
            }
        };
    }

    struct asserts
    {
        template <typename T>
        static bool notCalled(const T &spy)
        {
            const detail::SpyData *spyData = spy.data();
            return spyData->callCount == 0;
        }

        template <typename T, typename... Args>
        static bool calledWith(const T &spy, Args... args)
        {
            const detail::SpyData *spyData = spy.data();
            std::vector<bool> checked = helper::for_each_in_tuple<std::vector<bool>>(
                std::tuple<Args...>(args...),
                [&](const auto &it, int i)
                {
                    return spyData->callArgs.size() > i && it(spyData->callArgs[i]);
                });
            bool ret = true;
            for (const auto &it : checked)
            {
                ret = ret && it;
            }
            return ret;
        }

        template <typename... T>
        static bool callOrder(const T &...spies)
        {
            using List = std::vector<std::chrono::steady_clock::time_point>;
            List checked = helper::for_each_in_tuple<List>(
                std::tuple<T...>(spies...),
                [&](const auto &spy, int)
                {
                    return spy.data()->lastCallTime;
                });
            bool ret = checked[0] != std::chrono::steady_clock::time_point::min();
            for (size_t i = 1; i < checked.size(); i++)
            {
                ret = ret && checked[i] > checked[i - 1];
            }
            if (!ret)
            {
                for (const auto &it : checked)
                    INFO(it.time_since_epoch().count());
            }
            return ret;
        }
    };
}

#endif
