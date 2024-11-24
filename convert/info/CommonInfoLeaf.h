#pragma once

#include <glog/logging.h>
#include <absl/types/optional.h>
#include <set>
#include <map>
#include <string>
#include <unordered_set>

#include "clang/AST/Expr.h"

#include "../Type.h"
#include "PrefixPair.h"
#include "CommonInfo.h"
#include "CommonInfoCore.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class Env;
class StrictRewriter;

/// 通过 bs_info_util 中的模板类创建的 common info。
class CommonInfoLeaf : public CommonInfoCore, public PrefixPair {
 public:
  using CommonInfoCore::CommonInfoCore;
  explicit CommonInfoLeaf(const std::string& prefix_adlog,
                          int common_info_value):
    PrefixPair(prefix_adlog),
    common_info_value_(common_info_value) {}

  explicit CommonInfoLeaf(const std::string& prefix_adlog,
                          int common_info_value,
                          const std::string& method_name):
    CommonInfoCore(method_name),
    PrefixPair(prefix_adlog),
    common_info_value_(common_info_value) {}

  int common_info_value() const { return common_info_value_; }
  void set_common_info_value(int v) { common_info_value_ = v; }

  CommonInfoType common_info_type() const { return common_info_type_; }
  void set_common_info_type(CommonInfoType common_info_type) { common_info_type_ = common_info_type; }

  void set_common_info_enum_name(const std::string& common_info_enum_name) {
    common_info_enum_name_.emplace(common_info_enum_name);
  }

  const absl::optional<std::string>& common_info_enum_name() const { return (common_info_enum_name_); }

  /// 必须判断是否 ready, 一旦 ready 就不允许再 update。
  /// 否则 leaf method 可能在 get_common_info_body_text 中会被再次访问，当 last detail 类型不一样会有问题。
  void update_method_name(const std::string& method_name) override;

  void update_size_method_name(const std::string& size_method_name) override;

  void set_list_loop_var(const std::string& list_loop_var) { list_loop_var_ = list_loop_var; }

  const absl::optional<std::string>& list_loop_var() const { return list_loop_var_; }

  const absl::optional<std::string>& name_value_alias() const { return name_value_alias_; }

  void set_name_value_alias(const std::string& name_value_alias) {
    name_value_alias_.emplace(name_value_alias);
  }

  virtual std::string get_exists_expr(Env* env_ptr) const;
  virtual std::string get_bs_scalar_exists_def(Env* env_ptr) const;

  virtual std::string get_bs_scalar_def(Env* env_ptr) const;
  virtual std::string get_bs_list_def(Env* env_ptr) const;
  virtual std::string get_bs_map_def(Env* env_ptr) const;

  virtual std::string get_bs_var_name(Env* env_ptr) const;
  virtual std::string get_bs_var_def_name(Env* env_ptr) const;

  std::string bs_enum_str_to_camel(const std::string bs_enum_str) const;

  /// 属性定义, no 来自模板参数
  /// BSFixedCommonInfo<int64_t> BSGetItemAdDspInfoCommonInfoAttr(no);
  /// get_functor_name 返回 BSGetItemAdDspInfoCommonInfoAttr。
  /// get_exists_functor_name 返回 BSHasItemAdDspInfoCommonInfoAttr。
  virtual std::string get_functor_name() const;
  virtual std::string get_exists_functor_name() const;

  virtual std::string get_bs_scalar_field_def(Env* env_ptr) const;
  virtual std::string get_bs_scalar_exists_field_def(Env* env_ptr) const;
  virtual std::string get_bs_list_field_def(Env* env_ptr) const;
  virtual std::string get_bs_map_field_def(Env* env_ptr) const;

  virtual std::string get_bs_enum_str() const { return ""; }
  virtual std::string get_functor_tmpl() const { return ""; }
  virtual std::string get_exists_functor_tmpl() const { return ""; }
  virtual std::string get_field_def_params() const { return ""; }
  virtual bool is_item_field(const std::string& bs_enum_str) const { return ""; }

  std::string get_adlog_field_str() const {
    return prefix_adlog_ + std::string(".key:") + std::to_string(common_info_value_);
  }

  void copy_except_int_value(CommonInfoLeaf* common_info_leaf);
  std::string replace_value_key(const std::string& s, int common_info_value) const;

 protected:
  int common_info_value_;

  absl::optional<std::string> list_loop_var_;
  absl::optional<std::string> name_value_alias_;
  CommonInfoType common_info_type_;
  absl::optional<std::string> common_info_enum_name_;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
