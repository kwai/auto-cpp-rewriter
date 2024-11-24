#include "../Tool.h"
#include "BinaryOpInfo.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

std::string BinaryOpInfo::left_expr_str() const {
  return stmt_to_string(left_expr_);
}

std::string BinaryOpInfo::right_expr_str() const {
  return stmt_to_string(right_expr_);
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
