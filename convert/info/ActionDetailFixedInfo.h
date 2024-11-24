#pragma once

#include <absl/types/optional.h>

#include <map>
#include <string>
#include <unordered_map>

#include "clang/AST/Expr.h"

#include "InfoBase.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class Env;
class ExprInfo;

/// action detail 的 action 通过模板参数传递, Extract 函数中不能确定。
/// prefix 和 field_name 都是 . 隔开的形式, 如 adlog.user_info.user_real_time_action。
/// 叶子节点一定是 BSRepeatedField。
class ActionDetailFixedInfo : public InfoBase {
 public:
  ActionDetailFixedInfo() = default;
  explicit ActionDetailFixedInfo(const std::string& prefix_adlog, const std::string& action);

  const std::string& prefix() const { return prefix_; }
  const std::string& prefix_adlog() const { return prefix_adlog_; }
  const std::string& action() const { return action_; }

  std::string get_exists_expr(Env* env_ptr, const std::string& field_name) const;
  std::string get_exists_field_def(Env* env_ptr, const std::string& field_name) const;

  std::string get_action_detail_exists_expr(Env* env_ptr) const;
  std::string get_action_detail_exists_field_def(Env* env_ptr) const;

  std::string get_bs_enum_str(const std::string& field_name) const;
  std::string get_bs_var_name(Env* env_ptr, const std::string& field_name) const;

  /// 属性定义, action 来自模板参数
  /// std::string prefix = "adlog.user_info.user_real_time_action.real_time_dsp_action_detail";
  /// BSFixedActionDetail<BSRepeated<int64_t>> BSGetRealTimeDspActionDetailPhotoId{prefix, action, "photo_id"};
  std::string get_bs_list_def(Env* env_ptr,
                              const std::string& field_name,
                              clang::QualType qual_type) const;

  std::string get_bs_list_field_def(Env* env_ptr,
                                    const std::string& field_name,
                                    clang::QualType qual_type) const;

  std::string get_functor_name(const std::string& field_name) const;
  std::string get_exists_functor_name(const std::string& field_name) const;

  std::string user_template_param(Env* env_ptr,
                                  const std::string& field_name) const;
  std::string user_template_param(Env* env_ptr,
                                  const std::string& field_name,
                                  const std::string& type_str) const;

 private:
  /// prefix_ 是 adlog.user_info.user_real_time_action 这种形式。
  std::string prefix_;

  std::string prefix_adlog_;
  std::string action_;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
