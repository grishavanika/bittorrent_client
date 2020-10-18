#pragma once

#include <leaf.hpp>
#include <asio.hpp>

// Adaptation of BOOST_LEAF_AUTO and BOOST_LEAF_CHECK to coroutines world.
//   - change `return` to `co_return`
//   - workaround with ambitious overloads: cast to leaf::error_id() explicitly
#define LEAF_CO_ASSIGN(v,r)\
    auto && BOOST_LEAF_TOKEN_PASTE2(boost_leaf_temp_, __LINE__) = r;\
    static_assert(::boost::leaf::is_result_type<typename std::decay<decltype(BOOST_LEAF_TOKEN_PASTE2(boost_leaf_temp_, __LINE__))>::type>::value, "The BOOST_LEAF_ASSIGN macro requires a result type as the second argument");\
    if( !BOOST_LEAF_TOKEN_PASTE2(boost_leaf_temp_, __LINE__) )\
        co_return ::boost::leaf::error_id(BOOST_LEAF_TOKEN_PASTE2(boost_leaf_temp_, __LINE__).error());\
    v = BOOST_LEAF_TOKEN_PASTE2(boost_leaf_temp_, __LINE__).value()

#define LEAF_CO_AUTO(v, r)\
    LEAF_CO_ASSIGN(auto && v, r)

#define LEAF_CO_CHECK(r)\
    {\
        auto && _r = r;\
        static_assert(::boost::leaf::is_result_type<typename std::decay<decltype(_r)>::type>::value, "BOOST_LEAF_CHECK requires a result type");\
        if( !_r )\
            co_return ::boost::leaf::error_id(_r.error());\
    }

// Copy-paste of as_result(asio::use_awaitable)
// with modifications to have as_leaf_result(asio::use_awaitable).

///////////////////////////////////////////////////////////////////////////////
// FROM outcome adapter:
//    https://www.boost.org/doc/libs/1_74_0/libs/outcome/doc/html/recipes/asio-integration.html
//    https://gist.github.com/cstratopoulos/901b5cdd41d07c6ce6d83798b09ecf9b/da584844f58353915dc2600fba959813f793b456
// outcome::result<> replaced with leaf::result<>.
// From asio-integration.html above:
//     Warning
//       The below involves a lot of ASIO voodoo.
//       NO SUPPORT WILL BE GIVEN HERE FOR THE ASIO CODE BELOW.
//       Please raise any questions or problems that you have with how to
//       implement this sort of stuff in ASIO on Stackoverflow #boost - asio.

template<typename CompletionToken>
struct as_leaf_result_t {
    CompletionToken token_;
};

template<typename CompletionToken>
inline as_leaf_result_t<std::decay_t<CompletionToken>>
as_leaf_result(CompletionToken&& completion_token)
{
    return as_leaf_result_t<std::decay_t<CompletionToken>>{
        std::forward<CompletionToken>(completion_token)};
}

namespace detail {

    // Class to adapt as_outcome_t as a completion handler
    template<typename Handler>
    struct leaf_result_handler {
        void operator()(const std::error_code& ec)
        {
            using Result = boost::leaf::result<void>;

            if (ec)
                handler_(boost::leaf::new_error(ec));
            else
                handler_(Result());
        }

        void operator()(std::exception_ptr ex)
        {
            using Result = boost::leaf::result<void>;

            if (ex)
                handler_(boost::leaf::new_error(ex));
            else
                handler_(Result());
        }

        template<typename T>
        void operator()(const std::error_code& ec, T t)
        {
            using Result = boost::leaf::result<T>;

            if (ec)
                handler_(boost::leaf::new_error(ec));
            else
                handler_(Result(std::move(t)));
        }

        template<typename T>
        void operator()(std::exception_ptr ex, T t)
        {
            using Result = boost::leaf::result<T>;
            if (ex)
                handler_(boost::leaf::new_error(ex));
            else
                handler_(Result(std::move(t)));
        }

        Handler handler_;
    };

    template<typename Handler>
    inline void*
        asio_handler_allocate(std::size_t size, leaf_result_handler<Handler>* this_handler)
    {
        return asio_handler_alloc_helpers::allocate(size, this_handler->handler_);
    }

    template<typename Handler>
    inline void asio_handler_deallocate(
        void* pointer, std::size_t size, leaf_result_handler<Handler>* this_handler)
    {
        asio_handler_alloc_helpers::deallocate(pointer, size, this_handler->handler_);
    }

    template<typename Handler>
    inline bool asio_handler_is_continuation(leaf_result_handler<Handler>* this_handler)
    {
        return asio_handler_cont_helpers::is_continuation(this_handler->handler_);
    }

    template<typename Function, typename Handler>
    inline void
        asio_handler_invoke(Function& function, leaf_result_handler<Handler>* this_handler)
    {
        asio_handler_invoke_helpers::invoke(function, this_handler->handler_);
    }

    template<typename Function, typename Handler>
    inline void asio_handler_invoke(
        const Function& function, leaf_result_handler<Handler>* this_handler)
    {
        asio_handler_invoke_helpers::invoke(function, this_handler->handler_);
    }

    template<typename Signature>
    struct result_signature;

    template<>
    struct result_signature<void(std::error_code)> {
        using type = void(boost::leaf::result<void>);
    };

    template<>
    struct result_signature<void(const std::error_code&)>
        : result_signature<void(std::error_code)> {};

    template<>
    struct result_signature<void(std::exception_ptr)> {
        using type = void(boost::leaf::result<void>);
    };

    template<typename T>
    struct result_signature<void(std::error_code, T)> {
        using type = void(boost::leaf::result<T>);
    };

    template<typename T>
    struct result_signature<void(const std::error_code&, T)>
        : result_signature<void(std::error_code, T)> {};

    template<typename T>
    struct result_signature<void(std::exception_ptr, T)> {
        using type = void(boost::leaf::result<T>);
    };

    template<typename Signature>
    using result_signature_t = typename result_signature<Signature>::type;

} // namespace detail

namespace asio {

    template<typename CompletionToken, typename Signature>
    class async_result<as_leaf_result_t<CompletionToken>, Signature> {
    public:
        using result_signature = ::detail::result_signature_t<Signature>;

        using return_type =
            typename async_result<CompletionToken, result_signature>::return_type;

        template<typename Initiation, typename... Args>
        static return_type
            initiate(Initiation&& initiation, as_leaf_result_t<CompletionToken> token, Args&&... args)
        {
            return async_initiate<CompletionToken, result_signature>(
                [init = std::forward<Initiation>(initiation)](
                    auto&& handler, auto&&... callArgs) mutable {
                std::move(init)(
                    ::detail::leaf_result_handler<std::decay_t<decltype(handler)>>{
                    std::forward<decltype(handler)>(handler)},
                    std::forward<decltype(callArgs)>(callArgs)...);
            },
                token.token_,
                std::forward<Args>(args)...);
        }
    };

    template<typename Handler, typename Executor>
    struct associated_executor<::detail::leaf_result_handler<Handler>, Executor> {
        typedef typename associated_executor<Handler, Executor>::type type;

        static type
            get(const ::detail::leaf_result_handler<Handler>& h,
                const Executor& ex = Executor()) noexcept
        {
            return associated_executor<Handler, Executor>::get(h.handler_, ex);
        }
    };

    template<typename Handler, typename Allocator>
    struct associated_allocator<::detail::leaf_result_handler<Handler>, Allocator> {
        typedef typename associated_allocator<Handler, Allocator>::type type;

        static type
            get(const ::detail::leaf_result_handler<Handler>& h,
                const Allocator& a = Allocator()) noexcept
        {
            return associated_allocator<Handler, Allocator>::get(h.handler_, a);
        }
    };

} // namespace asio
