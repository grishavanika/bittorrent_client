//////////////////////////////////////////////
// Copy-paste from https://www.boost.org/doc/libs/1_74_0/libs/outcome/doc/html/recipes/asio-integration.html
// https://gist.github.com/cstratopoulos/901b5cdd41d07c6ce6d83798b09ecf9b/da584844f58353915dc2600fba959813f793b456
// 
#pragma once

/*
A Boost.ASIO async_result adapter for outcome (https://github.com/ned14/outcome)
Sample use:
    auto token = co_await boost::asio::experimental::this_coro::token();
    
    outcome::result<std::size_t> read = co_await beast::http::async_read(stream, buffer, request, as_result(token));
*/

#include <outcome.hpp>
#include <asio.hpp>

#include <type_traits>
#include <utility>
#include <system_error>

namespace outcome = OUTCOME_V2_NAMESPACE;

template<typename CompletionToken>
struct as_outcome_result_t {
    CompletionToken token_;
};

template<typename CompletionToken>
inline as_outcome_result_t<std::decay_t<CompletionToken>>
    as_result(CompletionToken&& completion_token)
{
    return as_outcome_result_t<std::decay_t<CompletionToken>>{
        std::forward<CompletionToken>(completion_token)};
}

namespace detail {

    // Class to adapt as_outcome_t as a completion handler
    template<typename Handler>
    struct outcome_result_handler {
        void operator()(const std::error_code& ec)
        {
            using Result = outcome::result<void, std::error_code>;

            if (ec)
                handler_(Result(outcome::failure(ec)));
            else
                handler_(Result(outcome::success()));
        }

        void operator()(std::exception_ptr ex)
        {
            using Result = outcome::result<void, std::exception_ptr>;

            if (ex)
                handler_(Result(outcome::failure(ex)));
            else
                handler_(Result(outcome::success()));
        }

        template<typename T>
        void operator()(const std::error_code& ec, T t)
        {
            using Result = outcome::result<T, std::error_code>;

            if (ec)
                handler_(Result(outcome::failure(ec)));
            else
                handler_(Result(outcome::success(std::move(t))));
        }

        template<typename T>
        void operator()(std::exception_ptr ex, T t)
        {
            using Result = outcome::result<T, std::exception_ptr>;

            if (ex)
                handler_(Result(outcome::failure(ex)));
            else
                handler_(Result(outcome::success(std::move(t))));
        }

        Handler handler_;
    };

    template<typename Handler>
    inline bool asio_handler_is_continuation(outcome_result_handler<Handler>* this_handler)
    {
        return asio_handler_cont_helpers::is_continuation(this_handler->handler_);
    }

    template<typename Signature>
    struct result_signature;

    template<>
    struct result_signature<void(std::error_code)> {
        using type = void(outcome::result<void, std::error_code>);
    };

    template<>
    struct result_signature<void(const std::error_code&)>
        : result_signature<void(std::error_code)> {};

    template<>
    struct result_signature<void(std::exception_ptr)> {
        using type = void(outcome::result<void, std::exception_ptr>);
    };

    template<typename T>
    struct result_signature<void(std::error_code, T)> {
        using type = void(outcome::result<T, std::error_code>);
    };

    template<typename T>
    struct result_signature<void(const std::error_code&, T)>
        : result_signature<void(std::error_code, T)> {};

    template<typename T>
    struct result_signature<void(std::exception_ptr, T)> {
        using type = void(outcome::result<T, std::exception_ptr>);
    };

    template<typename Signature>
    using result_signature_t = typename result_signature<Signature>::type;

} // namespace detail

namespace asio {

    template<typename CompletionToken, typename Signature>
    class async_result<as_outcome_result_t<CompletionToken>, Signature> {
    public:
        using result_signature = ::detail::result_signature_t<Signature>;

        using return_type =
            typename async_result<CompletionToken, result_signature>::return_type;

        template<typename Initiation, typename... Args>
        static return_type
            initiate(Initiation&& initiation, as_outcome_result_t<CompletionToken> token, Args&&... args)
        {
            return async_initiate<CompletionToken, result_signature>(
                [init = std::forward<Initiation>(initiation)](
                    auto&& handler, auto&&... callArgs) mutable {
                std::move(init)(
                    ::detail::outcome_result_handler<std::decay_t<decltype(handler)>>{
                    std::forward<decltype(handler)>(handler)},
                    std::forward<decltype(callArgs)>(callArgs)...);
            },
                token.token_,
                std::forward<Args>(args)...);
        }
    };

    template<typename Handler, typename Executor>
    struct associated_executor<::detail::outcome_result_handler<Handler>, Executor> {
        typedef typename associated_executor<Handler, Executor>::type type;

        static type
            get(const ::detail::outcome_result_handler<Handler>& h,
                const Executor& ex = Executor()) noexcept
        {
            return associated_executor<Handler, Executor>::get(h.handler_, ex);
        }
    };

    template<typename Handler, typename Allocator>
    struct associated_allocator<::detail::outcome_result_handler<Handler>, Allocator> {
        typedef typename associated_allocator<Handler, Allocator>::type type;

        static type
            get(const ::detail::outcome_result_handler<Handler>& h,
                const Allocator& a = Allocator()) noexcept
        {
            return associated_allocator<Handler, Allocator>::get(h.handler_, a);
        }
    };

} // namespace asio
