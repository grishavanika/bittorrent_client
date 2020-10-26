#pragma once
#include "utils_outcome.h"

#include <system_error>

enum class ClientErrorc : int;
std::error_code make_error_code(ClientErrorc);

namespace std
{
    template <>
    struct is_error_code_enum<ClientErrorc> : true_type {};
} // namespace std

enum class ClientErrorc : int
{
    Ok = 0,
    TODO,
};
