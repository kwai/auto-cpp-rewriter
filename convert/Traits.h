#pragma once

#include <type_traits>

namespace ks {
namespace ad_algorithm {
namespace convert {

template<typename T, T t>
using constant_t = std::integral_constant<T, t>;

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
