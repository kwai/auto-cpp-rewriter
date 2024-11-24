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

/// GetQueryToken 相关逻辑。
/// 详细逻辑见: docs/query_token.md
class QueryTokenRule: public RuleBase {
 public:
  using RuleBase::process;
  explicit QueryTokenRule(clang::Rewriter& rewriter): RuleBase(rewriter, "QueryTokenRule") {}   // NOLINT

  void process(clang::IfStmt* if_stmt, Env* env_ptr) override;
  void process(clang::BinaryOperator* binary_operator, Env* env_ptr) override;
  void process(clang::MemberExpr* member_expr, Env* env_ptr) override;
  void process(clang::CXXMemberCallExpr* cxx_member_call_expr, Env* env_ptr) override;
  void process(clang::ForStmt* for_stmt, Env* env_ptr) override;
  void process(clang::CXXForRangeStmt* cxx_for_range_stmt, Env* env_ptr) override;
  void process(clang::CXXOperatorCallExpr* cxx_operator_call_expr, Env* env_ptr) override;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
