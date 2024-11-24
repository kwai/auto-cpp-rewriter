#pragma once

#include <absl/types/optional.h>
#include <glog/logging.h>
#include <string>
#include <set>
#include <map>
#include <unordered_set>

#include "clang/AST/Expr.h"

#include "../Type.h"
#include "PrefixPair.h"
#include "CommonInfo.h"
#include "CommonInfoPrepare.h"
#include "CommonInfoBodyText.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class Env;
class StrictRewriter;

class CommonInfoCore : public CommonInfoBodyText {
 public:
  CommonInfoCore() = default;
  explicit CommonInfoCore(const std::string& method_name);

  bool is_ready() const { return is_ready_; }

  /// CommonInfoMultiMap 和 CommonInfoFixed 会用到
  const std::string& method_name() const { return method_name_; }
  void set_method_name(const std::string& method_name) { method_name_ = method_name; }

  const absl::optional<std::string>& size_method_name() const { return size_method_name_; }

  const CommonInfoValueType& value_type() const { return value_type_; }
  void set_value_type(CommonInfoValueType value_type) { value_type_ = value_type; }

  virtual void update_method_name(const std::string& method_name) {
    set_method_name(method_name);
    if (absl::optional<CommonInfoValueType> value_type = CommonAttrInfo::find_value_type(method_name)) {
      set_value_type(*value_type);
    }
  }

  virtual void update_size_method_name(const std::string& size_method_name) {
    std::string method_name =
      size_method_name.substr(0, size_method_name.size() - std::string("_size").size());
    set_method_name(method_name);
    if (absl::optional<CommonInfoValueType> value_type = CommonAttrInfo::find_value_type(method_name)) {
      set_value_type(*value_type);
    }
    size_method_name_.emplace(size_method_name);
  }

  bool is_for_stmt() const { return is_for_stmt_; }
  void set_is_for_stmt(bool v) { is_for_stmt_ = v; }

  bool is_scalar() const;
  bool is_list() const;
  bool is_map() const;
  bool is_list_size() const;
  bool is_map_size() const;

  /// size_method 是否是出现在 for 循环的初始化中用来遍历, 还是只被用来当做变量。
  bool is_size_method_in_loop_init() const { return is_size_method_in_loop_init_; }
  void set_is_size_method_in_loop_init(bool v) { is_size_method_in_loop_init_ = v; }

  bool has_list_method_address() const { return has_list_method_address_; }
  void set_has_list_method_address(bool v) { has_list_method_address_ = v; }

  const absl::optional<std::string> &list_loop_var_type() const {return (list_loop_var_type_);}
  void set_list_loop_var_type(const std::string &list_loop_var_type) {
    list_loop_var_type_.emplace(list_loop_var_type);
  }

  std::string get_list_inner_type_str(CommonInfoValueType value_type) const;

  const absl::optional<int>& compare_list_size_value() const { return (compare_list_size_value_); }
  void set_compare_list_size_vlaue(int v) { compare_list_size_value_.emplace(v); }

  const absl::optional<int>& list_size_dividend() const { return (list_size_dividend_); }
  void set_list_size_dividend(int v) { list_size_dividend_.emplace(v); }

 protected:
  std::string method_name_;

  absl::optional<std::string> size_method_name_;

  CommonInfoValueType value_type_;

  bool is_ready_ = false;

  /// for list method
  bool is_for_stmt_ = false;

  bool is_size_method_in_loop_init_ = false;

  bool has_list_method_address_ = false;

  absl::optional<std::string> list_loop_var_type_;

  absl::optional<int> compare_list_size_value_;

  absl::optional<int> list_size_dividend_;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
