#pragma once

#include <glog/logging.h>
#include <absl/types/optional.h>
#include <set>
#include <map>
#include <string>
#include <unordered_set>

#include "clang/AST/Expr.h"

#include "../Type.h"
#include "CommonInfo.h"
#include "MiddleNodeInfo.h"
#include "CommonInfoPrepare.h"
#include "PrefixPair.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class Env;
class StrictRewriter;

/// common info value 是模板参数, 用 BSFixedCommonInfo
class CommonInfoFixed : public CommonInfoCore, public PrefixPair {
 public:
  explicit CommonInfoFixed(const std::string& prefix_adlog,
                           const std::string& name):
    PrefixPair(prefix_adlog),
    int_name_(name) {}

  explicit CommonInfoFixed(const std::string& prefix_adlog,
                           const std::string& name,
                           const absl::optional<MiddleNodeInfo>& middle_node_info):
    PrefixPair(prefix_adlog),
    int_name_(name),
    middle_node_info_(middle_node_info) {}

  const std::string& int_name() const { return int_name_; }
  void set_int_name(const std::string& int_name) { int_name_ = int_name; }

  void set_list_loop_var(const std::string& list_loop_var) { list_loop_var_ = list_loop_var; }
  const absl::optional<std::string>& list_loop_var() const { return list_loop_var_; }

  std::string get_bs_wrap_text(Env* env_ptr) const;

  std::string get_exists_expr(Env* env_ptr) const;
  std::string get_bs_enum_str() const;

  std::string get_bs_scalar_def(Env* env_ptr) const;
  std::string get_bs_list_def(Env* env_ptr) const;
  std::string get_bs_map_def(Env* env_ptr) const;

  std::string get_bs_var_name(Env* env_ptr) const;
  std::string get_bs_var_def_name(Env *env_ptr) const;
  std::string get_map_bs_var_name(Env* env_ptr, const std::string& member) const;

  /// 属性定义, no 来自模板参数
  /// BSFixedCommonInfo<int64_t> BSGetItemAdDspInfoCommonInfoAttr(no);
  /// get_functor_name 返回 BSGetItemAdDspInfoCommonInfoAttr。
  /// get_exists_functor_name 返回 BSHasItemAdDspInfoCommonInfoAttr。
  std::string get_functor_name() const;

  std::string get_exists_functor_name() const;

  std::string get_bs_scalar_field_def(Env* env_ptr) const;
  std::string get_bs_scalar_exists_field_def(Env* env_ptr) const;
  std::string get_bs_list_field_def(Env* env_ptr) const;
  std::string get_bs_map_field_def(Env* env_ptr) const;

  std::string get_bs_scalar_field_value_expr(Env* env_ptr) const;

 private:
  /// 模板参数变量名
  std::string int_name_;

  absl::optional<std::string> list_loop_var_;

  absl::optional<MiddleNodeInfo> middle_node_info_;

  const std::string common_info_camel_ = "CommonInfo";
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
