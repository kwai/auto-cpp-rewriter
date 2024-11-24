#pragma once

#include <string>
#include <vector>
#include <set>
#include <list>
#include <unordered_set>

#include "clang/AST/Expr.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class Env;
class ExprInfo;

/// 处理最终需要删掉的表达式, 如
/// auto live_info = GetLiveInfo(adlog.item(pos));
/// auto it = user_attr_map_.find(user_attr.name_value());
class Deleter {
 public:
  static bool need_delete(ExprInfo* expr_info_ptr, Env* env_ptr);
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
