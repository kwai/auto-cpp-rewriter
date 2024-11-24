#pragma once

#include <glog/logging.h>
#include <absl/types/optional.h>
#include <map>
#include <string>
#include <vector>
#include <unordered_set>

#include "clang/AST/Stmt.h"
#include "clang/AST/Expr.h"

#include "../Type.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

/// CommonInfo 在确定类型之前需要保存的信息。
/// 几种 CommonInfo 可能都会用到。
class CommonInfoPrepare {
 public:
  CommonInfoPrepare() = default;
  explicit CommonInfoPrepare(const std::string& prefix_adlog);

  bool is_confirmed() const { return is_confirmed_; }
  void set_is_confirmed() { is_confirmed_ = true; }

  const absl::optional<std::string>& prefix() const { return (prefix_); }
  void set_prefix(const std::string& prefix) { prefix_.emplace(prefix); }

  const absl::optional<std::string>& prefix_adlog() const { return (prefix_adlog_); }
  void set_prefix_adlog(const std::string& prefix_adlog) { prefix_adlog_.emplace(prefix_adlog); }

  const absl::optional<std::string>& name_value_alias() const { return (name_value_alias_); }
  void set_name_value_alias(const std::string& name_value_alias) {
    name_value_alias_.emplace(name_value_alias);
  }

  const absl::optional<std::string>& method_name() const { return (method_name_); }
  void set_method_name(const std::string& method_name) { method_name_.emplace(method_name); }

  const absl::optional<std::string> &size_method_name() const {
    return (size_method_name_);
  }

  void set_size_method_name(const std::string &size_method_name) {
    size_method_name_.emplace(size_method_name);
  }

  void update_size_method_name(const std::string &size_method_name);

  const absl::optional<int>& int_value() const { return (int_value_); }
  void set_int_value(int v) { int_value_.emplace(v); }

  void add_other_if_stmt(clang::IfStmt* other_if_stmt) { other_if_stmts_.push_back(other_if_stmt); }
  const std::vector<clang::IfStmt*>& other_if_stmts() const { return other_if_stmts_; }

  void add_other_if_stmt_str(const std::string& other_if_stmt_str) {
    other_if_stmt_strs_.push_back(other_if_stmt_str);
  }
  const std::vector<std::string>& other_if_stmt_strs() const { return (other_if_stmt_strs_); }

  void add_other_decl_stmt_str(const std::string &other_decl_stmt_str) {
    other_decl_stmt_strs_.push_back(other_decl_stmt_str);
  }
  const std::vector<std::string> &other_decl_stmt_strs() const {
    return (other_decl_stmt_strs_);
  }

  bool is_for_stmt() const { return is_for_stmt_; }
  void set_is_for_stmt(bool v) { is_for_stmt_ = v; }


  void add_template_int_name(const std::string& template_int_name) {
    template_int_names_.push_back(template_int_name);
  }
  void add_common_info_value(int v) { common_info_values_.push_back(v); }

  const std::vector<std::string>& template_int_names() const { return (template_int_names_); }
  const std::vector<int>& common_info_values() const { return (common_info_values_); }

  void set_template_int_names(const std::vector<std::string>& int_names) { template_int_names_ = int_names; }
  void set_common_info_values(const std::vector<int>& values) { common_info_values_ = values; }

  bool is_common_info_normal() const;
  bool is_common_info_fixed_list() const;

  const absl::optional<std::string> &attr_name() const { return (attr_name_); }
  void set_attr_name(const std::string &attr_name) {attr_name_.emplace(attr_name);}

 private:
  bool is_confirmed_ = false;
  absl::optional<std::string> prefix_;
  absl::optional<std::string> prefix_adlog_;
  absl::optional<std::string> name_value_alias_;
  absl::optional<std::string> method_name_;
  absl::optional<std::string> size_method_name_;
  absl::optional<int> int_value_;
  std::vector<clang::IfStmt*> other_if_stmts_;
  std::vector<std::string> other_if_stmt_strs_;
  bool is_for_stmt_ = false;

  std::vector<std::string> other_decl_stmt_strs_;

  /// 用于在 Overrivew 中记录，区分是 CommonInfoNormal 还是 CommonInfoFixedList。
  /// 如果出现模板参数，则肯定是 CommonInfoFixedList。
  std::vector<std::string> template_int_names_;
  std::vector<int> common_info_values_;

  /// 变量名
  absl::optional<std::string> attr_name_;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
