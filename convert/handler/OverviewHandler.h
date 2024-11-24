#pragma once

#include <vector>
#include <memory>

#include "clang/AST/AST.h"
#include "clang/AST/Expr.h"
#include "clang/AST/Stmt.h"
#include "clang/Basic/SourceLocation.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class Env;

// 获取一些摘要信息，在改写之前
class OverviewHandler {
 public:
  OverviewHandler() = default;

  template<typename T>
  void process(T t, Env* env_ptr) {}

  void process(clang::CXXMemberCallExpr* cxx_member_call_expr, Env* env_ptr);
  void process(clang::CXXForRangeStmt* cxx_for_range_stmt, Env* env_ptr);
  void process(clang::CXXOperatorCallExpr* cxx_operator_call_expr, Env* env_ptr);
  void process(clang::BinaryOperator* binary_operator, Env* env_ptr);
  void process(clang::CallExpr* call_expr, Env* env_ptr);
  void process(clang::DeclRefExpr* decl_ref_expr, Env* env_ptr);
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
