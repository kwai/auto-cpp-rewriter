#pragma once

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/Tooling.h"
// Declares llvm::cl::extrahelp.
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/AST/Expr.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class FeatureInfo;

/// 处理 ad_n/utils/item_filter.h 中 ItemFilter 的函数。
/// 注意: 此处的 ItemFilter 不是训练用的 ItemFilter，训练用的 ItemFilter 是指
/// ad_algorithm/log_preprocess/item_filter 目录下的。
///
/// 示例:
/// static inline bool OcpxActionTypeFilter(const ItemAdaptorBase& item,
///                                         const FilterCondition& filter_condition) {
///   int ocpc_action_type = item.ad_dsp_info().unit().base().ocpc_action_type();
///   if (filter_condition.ocpx_action_type_set.count(ocpc_action_type) == 0) {
///     return true;
///   }
///   return false;
/// }
///
/// 函数名不同，但是接口相同，参数都是 item 和 filter_condition。
/// 需要将来自 item 的字段都认为是来自 adlog。
class InferFilterCallback : public clang::ast_matchers::MatchFinder::MatchCallback {
 public:
  InferFilterCallback() = default;
  explicit InferFilterCallback(clang::Rewriter &rewriter): rewriter_(rewriter) {}     // NOLINT

  void run(const clang::ast_matchers::MatchFinder::MatchResult &Result);

  void process_all_methods(const clang::CXXRecordDecl* cxx_record_decl, FeatureInfo* feature_info_ptr);

 private:
  clang::Rewriter& rewriter_;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
