#include "../Env.h"
#include "../Tool.h"
#include "LoopInfo.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

std::string LoopInfo::loop_var_expr_str() const {
  if (loop_var_expr_ != nullptr) {
    return tool::trim_this(stmt_to_string(loop_var_expr_));
  }

  return "";
}

std::string LoopInfo::origin_stmt_str() const {
  if (cxx_for_range_stmt_ != nullptr) {
    return stmt_to_string(cxx_for_range_stmt_);
  }

  return stmt_to_string(for_stmt_);
}

bool LoopInfo::is_double_list_loop() const {
  return is_double_list_inner_loop() || is_double_list_outer_loop();
}

bool LoopInfo::is_double_list_inner_loop() const {
  if (env_ptr_ != nullptr &&
      env_ptr_->is_parent_loop() &&
      !is_repeated_common_info() &&
      is_proto_list_loop() &&
      !is_for_stmt()) {
    return true;
  }

  return false;
}

bool LoopInfo::is_double_list_outer_loop() const {
  if (!is_repeated_common_info() &&
      is_proto_list_loop() &&
      is_child_proto_list_loop() &&
      !is_for_stmt()) {
    return true;
  }

  return false;
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
