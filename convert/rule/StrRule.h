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
class ExprInfo;

/// str 相关逻辑。
class StrRule: public RuleBase {
 public:
  using RuleBase::process;
  explicit StrRule(clang::Rewriter& rewriter): RuleBase(rewriter, "StrRule") {}    // NOLINT

  void process(clang::CallExpr* call_expr, Env* env_ptr) override;
  void process(clang::CXXMemberCallExpr* cxx_member_call_expr, Env* env_ptr) override;

  /// 替换赋值运算右边的参数。
  void process(clang::BinaryOperator* binary_operator, Env *env_ptr) override;
  void process(clang::CXXOperatorCallExpr* cxx_operator_call_expr, Env *env_ptr) override;

 private:
  void process_str_param_call(ExprInfo* expr_info_ptr, Env* env_ptr);

  void process_str_assign(clang::SourceRange source_range,
                          ExprInfo* left_expr_info,
                          ExprInfo* right_expr_info);
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
