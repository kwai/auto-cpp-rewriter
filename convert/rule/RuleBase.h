#pragma once

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <algorithm>
#include <iostream>
#include <list>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../Env.h"
#include "../handler/StrictRewriter.h"
#include "clang/AST/AST.h"
#include "clang/AST/Expr.h"
#include "clang/AST/Stmt.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Rewrite/Core/Rewriter.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class RuleBase {
 public:
  explicit RuleBase(clang::Rewriter& rewriter, const std::string& rule_name):      // NOLINT
    rewriter_(rewriter, rule_name) {}

  const std::string& name() const { return name_; }

  virtual void process(clang::Stmt* stmt, Env* env_ptr) {}
  virtual void process(clang::IfStmt* stmt, Env* env_ptr) {}
  virtual void process(clang::Expr* expr, Env* env_ptr) {}
  virtual void process(clang::DeclStmt* decl_stmt, Env* env_ptr) {}
  virtual void process(clang::CallExpr* call_expr, Env* env_ptr) {}
  virtual void process(clang::CXXMemberCallExpr* cxx_member_call_expr, Env* env_ptr) {}
  virtual void process(clang::MemberExpr* member_expr, Env* env_ptr) {}
  virtual void process(clang::UnaryOperator* unary_operator, Env* env_ptr) {}
  virtual void process(clang::BinaryOperator* binary_operator, Env* env_ptr) {}
  virtual void process(clang::CXXOperatorCallExpr* cxx_operator_call_expr, Env* env_ptr) {}
  virtual void process(clang::ForStmt* for_stmt, Env* env_ptr) {}
  virtual void process(clang::CXXForRangeStmt* cxx_for_range_stmt, Env* env_ptr) {}
  virtual void process(clang::DeclRefExpr* decl_ref_expr, Env* env_ptr) {}
  virtual void process(clang::CXXNullPtrLiteralExpr* cxx_null_ptr_literal_expr, Env* env_ptr) {}
  virtual void process(clang::GNUNullExpr* gnu_null_expr, Env* env_ptr) {}
  virtual void process(clang::CXXThisExpr* cxx_this_expr, Env* env_ptr) {}
  virtual void process(clang::ReturnStmt* return_stmt, Env* env_ptr) {}
  virtual void process(clang::BreakStmt* break_stmt, Env* env_ptr) {}
  virtual void process(clang::ContinueStmt* continue_stmt, Env* env_ptr) {}
  virtual void process(clang::IntegerLiteral* integer_literal, Env* env_ptr) {}
  virtual void process(clang::SwitchStmt* switch_stmt, Env* env_ptr) {}
  virtual void process(clang::CaseStmt* case_stmt, Env* env_ptr) {}
  virtual void process(clang::ConstantExpr* constant_expr, Env* env_ptr) {}
  virtual void process(clang::CXXDependentScopeMemberExpr* cxx_dependent_scope_member_expr, Env* env_ptr) {}
  virtual void process(clang::ArraySubscriptExpr* array_subscript_expr, Env* env_ptr) {}
  virtual void process(clang::CXXFunctionalCastExpr* cxx_functional_cast_expr, Env* env_ptr) {}

  virtual json process_to_json(clang::Stmt *stmt, Env *env_ptr) { return json::array(); }
  virtual json process_to_json(clang::IfStmt *stmt, Env *env_ptr) { return json::array(); }
  virtual json process_to_json(clang::Expr *expr, Env *env_ptr) { return json::array(); }
  virtual json process_to_json(clang::DeclStmt *decl_stmt, Env *env_ptr) { return json::array(); }
  virtual json process_to_json(clang::CallExpr *call_expr, Env *env_ptr) { return json::array(); }
  virtual json process_to_json(clang::CXXMemberCallExpr *cxx_member_call_expr,
                               Env *env_ptr) { return json::array(); }

  virtual json process_to_json(clang::MemberExpr *member_expr, Env *env_ptr) { return json::array(); }
  virtual json process_to_json(clang::UnaryOperator *unary_operator, Env *env_ptr) { return json::array(); }
  virtual json process_to_json(clang::BinaryOperator *binary_operator, Env *env_ptr) { return json::array(); }
  virtual json process_to_json(clang::CXXOperatorCallExpr *cxx_operator_call_expr,
                               Env *env_ptr) { return json::array(); }

  virtual json process_to_json(clang::ForStmt *for_stmt, Env *env_ptr) { return json::array(); }
  virtual json process_to_json(clang::CXXForRangeStmt *cxx_for_range_stmt, Env *env_ptr) { return json::array(); }
  virtual json process_to_json(clang::DeclRefExpr *decl_ref_expr, Env *env_ptr) { return json::array(); }
  virtual json process_to_json(clang::CXXNullPtrLiteralExpr *cxx_null_ptr_literal_expr,
                               Env *env_ptr) { return json::array(); }
  virtual json process_to_json(clang::GNUNullExpr *gnu_null_expr, Env *env_ptr) { return json::array(); }
  virtual json process_to_json(clang::CXXThisExpr *cxx_this_expr, Env *env_ptr) { return json::array(); }
  virtual json process_to_json(clang::ReturnStmt *return_stmt, Env *env_ptr) { return json::array(); }
  virtual json process_to_json(clang::BreakStmt *break_stmt, Env *env_ptr) { return json::array(); }
  virtual json process_to_json(clang::ContinueStmt *continue_stmt, Env *env_ptr) { return json::array(); }
  virtual json process_to_json(clang::IntegerLiteral *integer_literal, Env *env_ptr) { return json::array(); }
  virtual json process_to_json(clang::SwitchStmt *switch_stmt, Env *env_ptr) { return json::array(); }
  virtual json process_to_json(clang::CaseStmt *case_stmt, Env *env_ptr) { return json::array(); }
  virtual json process_to_json(clang::ConstantExpr *constant_expr, Env *env_ptr) { return json::array(); }
  virtual json process_to_json(clang::CXXDependentScopeMemberExpr *cxx_dependent_scope_member_expr,
                               Env *env_ptr) { return json::array(); }
  virtual json process_to_json(clang::ArraySubscriptExpr *array_subscript_expr,
                               Env *env_ptr) { return json::array(); }
  virtual json process_to_json(clang::CXXFunctionalCastExpr *cxx_functional_cast_expr,
                               Env *env_ptr) { return json::array(); }

  std::string get_loop_body(clang::Stmt* body, bool skip_if = false);
  std::string get_loop_body_without_if(clang::Stmt* body);

  // 删掉多余变量
  std::string get_complete_rewritten_text(clang::Stmt* body, Env* env_ptr);
  std::string get_compound_stmt_rewritten_text(clang::Stmt* stmt);

  void process_deleted_var(Env* env_ptr);

 protected:
  StrictRewriter rewriter_;
  std::string name_;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
