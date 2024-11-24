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
#include "PrefixPair.h"
#include "MiddleNodeInfo.h"
#include "CommonInfoFixed.h"
#include "CommonInfoPrepare.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class Env;
class StrictRewriter;

/// 多个 common info value 是模板参数, 用 BSFixedCommonInfo
/// 与 CommonInfoNormal 逻辑类似，重复代码比较多，后面有时间再重构下。
class CommonInfoFixedList : public CommonAttrInfo, public PrefixPair {
 public:
  explicit CommonInfoFixedList(const std::string& prefix_adlog):
    CommonAttrInfo(CommonInfoType::FIXED),
    PrefixPair(prefix_adlog) {}

  explicit CommonInfoFixedList(const std::string& prefix_adlog,
                               const absl::optional<MiddleNodeInfo>& middle_node_info):
    CommonAttrInfo(CommonInfoType::FIXED),
    PrefixPair(prefix_adlog),
    middle_node_info_(middle_node_info) {}

  /// 注意，添加标准是遇到 attr.name_value() 判断时候添加，因此必须去重。
  /// 由于在 BinaryOperator 中，因此 attr.name_value() 可能会被访问多次。
  void add_int_name(const std::string& int_name);

  void set_list_loop_var(const std::string& list_loop_var) { list_loop_var_ = list_loop_var; }
  const absl::optional<std::string>& list_loop_var() const { return list_loop_var_; }

  const std::vector<std::shared_ptr<CommonInfoFixed>> & common_info_details() const {
    return (common_info_details_);
  }
  std::vector<std::shared_ptr<CommonInfoFixed>> &mutable_common_info_details() {
    return (common_info_details_);
  }

  const CommonInfoFixed*  get_common_info_detail(size_t index) const {
    if (index >= common_info_details_.size()) {
      return nullptr;
    }

    return common_info_details_[index].get();
  }

  CommonInfoFixed* mutable_common_info_detail(size_t index) {
    if (index >= common_info_details_.size()) {
      return nullptr;
    }

    return common_info_details_[index].get();
  }

  CommonInfoFixed* mutable_common_info_detail_by_int_name(const std::string& int_name);

  size_t common_info_details_size() const {
    return common_info_details_.size();
  }

  const CommonInfoFixed* last_common_info_detail() const {
    if (common_info_details_.size() == 0) {
      return nullptr;
    }
    return common_info_details_.back().get();
  }
  CommonInfoFixed* last_mutable_common_info_detail() {
    if (common_info_details_.size() == 0) {
      return nullptr;
    }
    return common_info_details_.back().get();
  }

  /// 列表遍历普通 for 循环的分 cxx_for_range_stmt 和 for_stmt 两种情况。
  /// cxx_for_range_stmt 可以直接替换, for_stmt
  /// 可能还有其他条件，因此必须基于原来的 for 循环替换。
  std::string get_bs_rewritten(StrictRewriter *rewriter_ptr, size_t index) const;
  std::string get_bs_wrap_text(const std::string &text) const;

  bool is_already_exists(const std::string& int_name);

  void add_other_if_stmt(clang::IfStmt* other_if_stmt) { other_if_stmts_.push_back(other_if_stmt); }

 private:
  /// 模板参数变量名
  std::vector<std::shared_ptr<CommonInfoFixed>> common_info_details_;
  absl::optional<std::string> list_loop_var_;
  absl::optional<MiddleNodeInfo> middle_node_info_;
  std::vector<clang::IfStmt*> other_if_stmts_;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
