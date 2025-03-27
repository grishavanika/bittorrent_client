#pragma once
#include <outcome/outcome.hpp>
#include <outcome/try.hpp>
namespace outcome = OUTCOME_V2_NAMESPACE;


// Same as OUTCOME_CO_TRY() except returns
// ERROR value (i.e., std::error_code).
// Useful when outcome::result<> can't be returned from the function.
// E.g., ASIO co_spawn() requires return type to be
// default-constructible.
#define QQ_GLUE2(X, ID) X ## ID
#define QQ_GLUE(X, ID) QQ_GLUE2(X, ID)

#define OUTCOME_CO_TRY_ERR(expr) \
    auto&& QQ_GLUE(rr_, __LINE__) = expr; \
    if (!QQ_GLUE(rr_, __LINE__)) { co_return QQ_GLUE(rr_, __LINE__).error(); } \
    (void)0

#define OUTCOME_CO_TRY_ERRV(V, expr) \
    auto&& QQ_GLUE(V_, __LINE__) = expr; \
    if (!QQ_GLUE(V_, __LINE__)) { co_return QQ_GLUE(V_, __LINE__).error(); } \
    auto&& V = QQ_GLUE(V_, __LINE__).value()
