#pragma once
#include "utils_outcome.h"
#include <asio.hpp>

template<typename T>
using co_asio_result = asio::awaitable<outcome::result<T>>;
