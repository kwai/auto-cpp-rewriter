#pragma once

#include <glog/logging.h>
#include <absl/types/optional.h>

#include "clang/AST/Expr.h"

#include "RuleBase.h"
#include "../handler/StrictRewriter.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class Env;

class CommonInfoRule: public RuleBase {
 public:
  using RuleBase::process;
  explicit CommonInfoRule(clang::Rewriter& rewriter): RuleBase(rewriter, "CommonInfoRule") {}  // NOLINT

  void process(clang::IfStmt* if_stmt, Env* env_ptr) override;
  void process(clang::ForStmt* for_stmt, Env* env_ptr) override;
  void process(clang::CXXForRangeStmt* cxx_for_range_stmt, Env* env_ptr) override;
  void process(clang::SwitchStmt* switch_stmt, Env* env_ptr) override;
  void process(clang::CaseStmt* case_stmt, Env* env_ptr) override;
  void process(clang::CXXOperatorCallExpr* cxx_operator_call_expr, Env* env_ptr) override;
  void process(clang::DeclStmt* decl_stmt, Env* env_ptr) override;
  void process(clang::CXXMemberCallExpr* cxx_member_call_expr, Env* env_ptr) override;
  void process(clang::CallExpr* call_expr, Env* env_ptr) override;
  void process(clang::BinaryOperator* binary_operator, Env* env_ptr) override;

  // common info 里的逻辑
  CommonInfoBodyText get_common_info_body_text(clang::Stmt* stmt, Env* env_ptr);
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
