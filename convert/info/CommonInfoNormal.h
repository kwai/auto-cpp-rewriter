#pragma once

#include <glog/logging.h>
#include <absl/types/optional.h>
#include <set>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <unordered_set>

#include "clang/AST/Expr.h"

#include "../Type.h"
#include "CommonInfo.h"
#include "CommonInfoLeaf.h"
#include "CommonInfoPrepare.h"
#include "PrefixPair.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class Env;
class StrictRewriter;

class CommonInfoNormal : public CommonAttrInfo, public PrefixPair {
 public:
  explicit CommonInfoNormal(const std::string& prefix_adlog):
    CommonAttrInfo(CommonInfoType::NORMAL),
    PrefixPair(prefix_adlog) {
  }

  explicit CommonInfoNormal(const std::string &prefix_adlog,
                            const std::string &middle_node_root) :
    CommonAttrInfo(CommonInfoType::MIDDLE_NODE),
    PrefixPair(prefix_adlog),
    middle_node_root_(middle_node_root) {}

  /// 注意，添加标准是遇到 attr.name_value() 判断时候添加，因此必须去重。
  /// 由于在 BinaryOperator 中，因此 attr.name_value() 可能会被访问多次。
  void add_common_info_value(int v);

  void set_list_loop_var(const std::string& list_loop_var) { list_loop_var_ = list_loop_var; }
  const absl::optional<std::string>& list_loop_var() const { return list_loop_var_; }

  const std::vector<std::shared_ptr<CommonInfoLeaf>>& common_info_details() const {
    return (common_info_details_);
  }
  std::vector<std::shared_ptr<CommonInfoLeaf>>& mutable_common_info_details() {
    return (common_info_details_);
  }

  const std::shared_ptr<CommonInfoLeaf>& get_common_info_detail(size_t index) const {
    return (common_info_details_[index]);
  }
  std::shared_ptr<CommonInfoLeaf>& mutable_common_info_detail(size_t index) {
    return common_info_details_[index];
  }

  std::shared_ptr<CommonInfoLeaf> mutable_common_info_detail_by_value(int value);

  size_t common_info_details_size() const { return common_info_details_.size(); }

  const std::shared_ptr<CommonInfoLeaf>& last_common_info_detail() const;
  std::shared_ptr<CommonInfoLeaf>& last_mutable_common_info_detail();

  void set_uni_method_name(const std::string& uni_method_name) { uni_method_name_ = uni_method_name; }
  const absl::optional<std::string>& uni_method_name() const { return uni_method_name_; }

  void add_other_if_stmt(clang::IfStmt* other_if_stmt) { other_if_stmts_.push_back(other_if_stmt); }

  /// 列表遍历普通 for 循环的分 cxx_for_range_stmt 和 for_stmt 两种情况。
  /// cxx_for_range_stmt 可以直接替换, for_stmt 可能还有其他条件，因此必须基于原来的 for 循环替换。
  std::string get_bs_rewritten(StrictRewriter* rewriter_ptr, size_t index) const;
  std::string get_bs_wrap_text(const std::string& text) const;

  bool is_check_equal() const { return is_check_equal_; }
  void set_is_check_equal(bool v) { is_check_equal_ = v; }

  const absl::optional<std::string>& name_value_alias() const { return name_value_alias_; }
  void set_name_value_alias(const std::string& name_value_alias) {
    name_value_alias_.emplace(name_value_alias);
  }

  bool is_already_exists(int v);

 protected:
  absl::optional<std::string> list_loop_var_;
  absl::optional<std::string> uni_method_name_;
  std::vector<clang::IfStmt*> other_if_stmts_;
  std::vector<std::shared_ptr<CommonInfoLeaf>> common_info_details_;
  bool is_check_equal_ = true;
  absl::optional<std::string> name_value_alias_;
  absl::optional<std::string> middle_node_root_;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
