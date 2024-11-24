#pragma once

#include <glog/logging.h>
#include <absl/types/optional.h>

#include "clang/AST/Expr.h"

#include "RuleBase.h"
#include "../Type.h"
#include "../handler/StrictRewriter.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class Env;

/// for 循环遍历的是中间的 proto list。
/// 详细逻辑见: docs/proto_list.md
class ProtoListRule: public RuleBase {
 public:
  using RuleBase::process;
  explicit ProtoListRule(clang::Rewriter& rewriter): RuleBase(rewriter, "ProtoListRule") {}   // NOLINT

  void process(clang::ForStmt* for_stmt, Env* env_ptr) override;
  void process(clang::CXXForRangeStmt* cxx_for_range_stmt, Env* env_ptr) override;
  void process(clang::CXXMemberCallExpr* cxx_member_call_expr, Env* env_ptr) override;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
