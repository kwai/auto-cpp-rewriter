#pragma once

#include <glog/logging.h>
#include <absl/types/optional.h>
#include <set>
#include <string>
#include <map>
#include <unordered_set>

#include "clang/AST/Expr.h"

#include "../Type.h"
#include "CommonInfoLeaf.h"
#include "CommonInfoPrepare.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class Env;
class StrictRewriter;

class CommonInfoDetail: public CommonInfoLeaf {
 public:
  explicit CommonInfoDetail(const std::string& prefix_adlog,
                            int common_info_value);
  explicit CommonInfoDetail(const std::string& prefix_adlog,
                            int common_info_value,
                            const std::string& method_name);
  /// 返回判断条件是否存在的 bs 表达式
  /// 如果是 scalar, 则返回 xxx_exists, xxx_exists 的定义需要提前放到 env_ptr 中
  /// 如果是 list 或者 map, 则返回 !attr.is_empty()
  std::string get_exists_expr(Env* env_ptr) const override;
  std::string get_bs_enum_str() const override;

  std::string get_bs_scalar_def(Env* env_ptr) const override;
  std::string get_bs_scalar_exists_def(Env* env_ptr) const override;
  std::string get_bs_list_def(Env* env_ptr) const override;
  std::string get_bs_map_def(Env* env_ptr) const override;

  std::string get_bs_var_name(Env* env_ptr) const override;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
