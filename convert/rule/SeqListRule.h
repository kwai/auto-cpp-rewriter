#pragma once

#include <glog/logging.h>
#include <absl/types/optional.h>

#include "clang/AST/Expr.h"

#include "../Type.h"
#include "RuleBase.h"
#include "../handler/StrictRewriter.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class Env;

/// 详细规则见: docs/get_seq_list.md
class SeqListRule: public RuleBase {
 public:
  using RuleBase::process;
  explicit SeqListRule(clang::Rewriter& rewriter): RuleBase(rewriter, "SeqListRule") {}   // NOLINT

  void process(clang::IfStmt* if_stmt, Env* env_ptr) override;

  /// 替换 repeated proto 为 BSRepeatedField
  /// 示例:
  /// const ::google::protobuf::RepeatedField<::google::protobuf::int64> *seq_list = nullptr;
  /// 替换为 BSRepeatedField<int64_t>
  void process(clang::DeclStmt *decl_stmt, Env *env_ptr) override;

  void process(clang::UnaryOperator* unary_operator, Env *env_ptr) override;
  void process(clang::BinaryOperator* binary_operator, Env *env_ptr) override;
  void process(clang::CallExpr *call_expr, Env *env_ptr) override;
  void process(clang::CXXMemberCallExpr* cxx_member_call_expr, Env* env_ptr) override;
  void process(clang::ReturnStmt* return_stmt, Env* env_ptr);
  void process(clang::CXXDependentScopeMemberExpr *cxx_dependent_scope_member_expr, Env *env_ptr);
  void process(clang::ArraySubscriptExpr *array_subscript_expr, Env *env_ptr);
  void process(clang::CXXForRangeStmt* cxx_for_range_stmt, Env *env_ptr);
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
