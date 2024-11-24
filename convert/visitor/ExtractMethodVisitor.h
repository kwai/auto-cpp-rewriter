#pragma once

#include <string>
#include <vector>

#include "clang/AST/ExprCXX.h"
#include "clang/AST/AST.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Basic/SourceLocation.h"

#include "../Env.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class ExtractMethodVisitor {
 public:
  explicit ExtractMethodVisitor(clang::Rewriter &rewriter) :           // NOLINT
    rewriter_(rewriter) {
  }

  void visit_params(const clang::CXXMethodDecl* cxx_method_decl, Env* env_ptr);
  void visit(const clang::CXXMethodDecl* cxx_method_decl, FeatureInfo* feature_info_ptr);

  void get_overview_info(FeatureInfo* feature_info_ptr,
                         const clang::CXXMethodDecl* cxx_method_decl,
                         const std::string& method_name);

  template<typename Handler>
  void recursive_visit(clang::Stmt *stmt,
                       Handler* handler_ptr,
                       Env* env_ptr);

  template<typename T, typename Handler>
  void visit_loop(T* loop_stmt,
                  Handler* handler_ptr,
                  Env* env_ptr);

  template<typename T, typename Handler>
  void visit_loop_range(T* loop_stmt,
                        Handler* handler_ptr,
                        Env* env_ptr);

  void visit_loop_init(clang::ForStmt* for_stmt,
                       Env* env_ptr);

  void visit_loop_init(clang::CXXForRangeStmt* cxx_for_range_stmt,
                       Env* env_ptr);

  void visit_loop_var_decl(clang::VarDecl* var_decl,
                           Env* env_ptr);

  void process_deleted_var(StrictRewriter* rewriter_ptr, Env* env_ptr);
  void process_get_bs(StrictRewriter* rewriter_ptr,
                      Env* env_ptr,
                      absl::optional<clang::SourceLocation> body_begin_loc,
                      clang::Stmt* body);
  void process_constructor(StrictRewriter* rewriter_ptr, Env* env_ptr);
  void process_update_action(StrictRewriter* rewriter_ptr, Env* env_ptr);
  void process_new_field_def(StrictRewriter* rewriter_ptr,
                             Env* env_ptr,
                             clang::SourceLocation extract_method_end);

  absl::optional<clang::SourceLocation> find_body_begin_loc(clang::Stmt* stmt);

  void reset_rewrite_buffer(Env* env_ptr);

  void process_params(const clang::CXXMethodDecl* cxx_method_decl,
                      StrictRewriter* rewriter_ptr,
                      Env* env_ptr,
                      std::vector<std::string>* params_text_ptr);

  void remove_text_before_first_if(clang::Stmt* body);
  std::string get_rewritten_text_before_first_if(clang::Stmt* body);

  /// 用于处理 infer 的 ItemFilter
  /// 如果第一个参数是 `const ItemAdaptorBase& item`, 第二个参数是 `const FilterCondition& filter_condition`,
  /// 则返回 true，否则返回 false。
  bool is_infer_filter_method(const clang::CXXMethodDecl* cxx_method_decl);
  /// 将 `const ItemAdaptorBase& item` 替换为 `const SampleInterface& bs`。
  void process_infer_filter_param(const clang::CXXMethodDecl* cxx_method_decl,
                                  StrictRewriter* rewriter_ptr);
  /// 将 root env 的变量添加到最开始。
  void process_infer_filter_root_env(StrictRewriter* rewriter_ptr,
                                     Env* env_ptr,
                                     absl::optional<clang::SourceLocation> body_begin_loc);

  /// 将最外层的新增变量定义添加到 body 中合适的位置。取包含所有变量的最早的行的前一行为其位置。
  ///
  /// 如
  ///     if (info_exists) { ... }
  ///
  /// 则将 info_exists 定义添加到 if 前。
  std::string insert_top_new_defs_to_body(
    const std::string& body_str,
    const std::vector<std::string>& new_def_var_names,
    const std::string& new_defs) const;

 protected:
  clang::Rewriter &rewriter_;
};

template<typename Handler>
void ExtractMethodVisitor::recursive_visit(clang::Stmt *stmt,
                                           Handler* handler_ptr,
                                           Env* env_ptr) {
  if (!stmt) {
    return;
  }

  if (clang::CompoundStmt* compound_stmt = dyn_cast<clang::CompoundStmt>(stmt)) {
    for (clang::CompoundStmt::body_iterator start = compound_stmt->body_begin();
         start != compound_stmt->body_end();
         start++) {
      recursive_visit(*start, handler_ptr, env_ptr);
      handler_ptr->process(*start, env_ptr);
    }

  } else if (clang::DeclStmt* decl_stmt = dyn_cast<clang::DeclStmt>(stmt)) {
    env_ptr->update(decl_stmt);
    if (clang::VarDecl* var_decl = dyn_cast<clang::VarDecl>(decl_stmt->getSingleDecl())) {
      if (var_decl->hasInit()) {
        recursive_visit(var_decl->getInit(), handler_ptr, env_ptr);
      }
      if (const clang::Expr* init_expr = var_decl->getAnyInitializer()) {
        recursive_visit(const_cast<clang::Expr*>(init_expr), handler_ptr, env_ptr);
      }
    }

    handler_ptr->process(decl_stmt, env_ptr);

    // decl_stmt 比较特殊, decl_info 只维持当前变量, 访问完当前 decl_stmt 后立即销毁。
    env_ptr->clear_decl_info();

  } else if (clang::CXXConstructExpr* cxx_construct_expr = dyn_cast<clang::CXXConstructExpr>(stmt)) {
    for (size_t i = 0; i < cxx_construct_expr->getNumArgs(); i++) {
      recursive_visit(cxx_construct_expr->getArg(i), handler_ptr, env_ptr);
    }

  } else if (clang::ExprWithCleanups* expr_with_cleanups = dyn_cast<clang::ExprWithCleanups>(stmt)) {
    recursive_visit(expr_with_cleanups->getSubExpr(), handler_ptr, env_ptr);

  } else if (clang::ImplicitCastExpr* implicit_cast_expr = dyn_cast<clang::ImplicitCastExpr>(stmt)) {
    recursive_visit(implicit_cast_expr->getSubExpr(), handler_ptr, env_ptr);

  } else if (clang::CXXBindTemporaryExpr* cxx_bind_temporary_expr =
             dyn_cast<clang::CXXBindTemporaryExpr>(stmt)) {
    recursive_visit(cxx_bind_temporary_expr->getSubExpr(), handler_ptr, env_ptr);

  } else if (clang::CXXNullPtrLiteralExpr* cxx_null_ptr_literal_expr =
             dyn_cast<clang::CXXNullPtrLiteralExpr>(stmt)) {
    handler_ptr->process(cxx_null_ptr_literal_expr, env_ptr);

  } else if (clang::CXXMemberCallExpr* cxx_member_call_expr = dyn_cast<clang::CXXMemberCallExpr>(stmt)) {
    for (unsigned i = 0; i < cxx_member_call_expr->getNumArgs(); i++) {
      recursive_visit(cxx_member_call_expr->getArg(i), handler_ptr, env_ptr);
    }
    handler_ptr->process(cxx_member_call_expr, env_ptr);

  } else if (clang::CXXOperatorCallExpr* cxx_operator_call_expr =
             dyn_cast<clang::CXXOperatorCallExpr>(stmt)) {
    env_ptr->update(cxx_operator_call_expr);
    for (size_t i = 0; i < cxx_operator_call_expr->getNumArgs(); i++) {
      recursive_visit(cxx_operator_call_expr->getArg(i), handler_ptr, env_ptr);
    }

    handler_ptr->process(cxx_operator_call_expr, env_ptr);
    env_ptr->clear_binary_op_info();

  } else if (clang::CallExpr* call_expr = dyn_cast<clang::CallExpr>(stmt)) {
    // 会包含 CXXMemberCallExpr 和 CXXOperatorCallExpr, 此处逻辑需要再仔细考虑下
    for (unsigned i = 0; i < call_expr->getNumArgs(); i++) {
      recursive_visit(call_expr->getArg(i), handler_ptr, env_ptr);
    }

    for (unsigned i = 0; i < call_expr->getNumArgs(); i++) {
      handler_ptr->process(call_expr->getArg(i), env_ptr);
    }

    handler_ptr->process(call_expr, env_ptr);

  } else if (clang::CXXDependentScopeMemberExpr *cxx_dependent_scope_member_expr =
           dyn_cast<clang::CXXDependentScopeMemberExpr>(stmt)) {
    handler_ptr->process(cxx_dependent_scope_member_expr, env_ptr);

  } else if (clang::ArraySubscriptExpr* array_subscript_expr = dyn_cast<clang::ArraySubscriptExpr>(stmt)) {
    handler_ptr->process(array_subscript_expr, env_ptr);

  } else if (clang::UnaryOperator *unary_operator = dyn_cast<clang::UnaryOperator>(stmt)) {
    recursive_visit(unary_operator->getSubExpr(), handler_ptr, env_ptr);
    handler_ptr->process(unary_operator, env_ptr);

  } else if (clang::MemberExpr* member_expr = dyn_cast<clang::MemberExpr>(stmt)) {
    handler_ptr->process(member_expr, env_ptr);

  } else if (clang::BinaryOperator* binary_operator = dyn_cast<clang::BinaryOperator>(stmt)) {
    env_ptr->update(binary_operator);
    if (binary_operator->isAssignmentOp()) {
      env_ptr->update_assign_info(binary_operator);
    }

    recursive_visit(binary_operator->getLHS(), handler_ptr, env_ptr);
    handler_ptr->process(binary_operator->getLHS(), env_ptr);

    recursive_visit(binary_operator->getRHS(), handler_ptr, env_ptr);
    handler_ptr->process(binary_operator->getRHS(), env_ptr);

    handler_ptr->process(binary_operator, env_ptr);

    env_ptr->clear_binary_op_info();
    if (binary_operator->isAssignmentOp()) {
      env_ptr->clear_assign_info();
    }

  } else if (clang::ParenExpr* paren_expr = dyn_cast<clang::ParenExpr>(stmt)) {
    recursive_visit(paren_expr->getSubExpr(), handler_ptr, env_ptr);

  } else if (clang::ImplicitCastExpr* implicit_cast_expr = dyn_cast<clang::ImplicitCastExpr>(stmt)) {
    recursive_visit(implicit_cast_expr->getSubExpr(), handler_ptr, env_ptr);

  } else if (clang::MaterializeTemporaryExpr* materialize_temporary_expr =
             dyn_cast<clang::MaterializeTemporaryExpr>(stmt)) {
    recursive_visit(materialize_temporary_expr->getSubExpr(), handler_ptr, env_ptr);

  } else if (clang::CXXThisExpr* cxx_this_expr = dyn_cast<clang::CXXThisExpr>(stmt)) {
    handler_ptr->process(cxx_this_expr, env_ptr);

  } else if (clang::ReturnStmt* return_stmt = dyn_cast<clang::ReturnStmt>(stmt)) {
    recursive_visit(return_stmt->getRetValue(), handler_ptr, env_ptr);

    handler_ptr->process(return_stmt, env_ptr);

  } else if (clang::BreakStmt* break_stmt = dyn_cast<clang::BreakStmt>(stmt)) {
    handler_ptr->process(break_stmt, env_ptr);

  } else if (clang::ContinueStmt* continue_stmt = dyn_cast<clang::ContinueStmt>(stmt)) {
    handler_ptr->process(continue_stmt, env_ptr);

  } else if (clang::IfStmt* if_stmt = dyn_cast<clang::IfStmt>(stmt)) {
    Env new_env(env_ptr);
    new_env.update(if_stmt);

    clang::Expr* cond_expr = if_stmt->getCond();
    cond_expr = cond_expr->IgnoreImpCasts();
    recursive_visit(cond_expr, handler_ptr, &new_env);

    clang::Stmt* then_expr = if_stmt->getThen();
    clang::Stmt* else_expr = if_stmt->getElse();

    auto& if_info = new_env.cur_mutable_if_info();
    if_info->set_if_stage(IfStage::THEN);
    recursive_visit(then_expr, handler_ptr, &new_env);

    if_info->set_if_stage(IfStage::ELSE);
    recursive_visit(else_expr, handler_ptr, &new_env);

    if_info->set_if_stage(IfStage::END);

    handler_ptr->process(if_stmt, &new_env);

  } else if (clang::ForStmt* for_stmt = dyn_cast<clang::ForStmt>(stmt)) {
    Env new_env(env_ptr);
    new_env.update(for_stmt);
    visit_loop(for_stmt, handler_ptr, &new_env);

  } else if (clang::CXXForRangeStmt* cxx_for_range_stmt = dyn_cast<clang::CXXForRangeStmt>(stmt)) {
    Env new_env(env_ptr);
    new_env.update(cxx_for_range_stmt);

    visit_loop(cxx_for_range_stmt, handler_ptr, &new_env);

  } else if (clang::DeclRefExpr* decl_ref_expr = dyn_cast<clang::DeclRefExpr>(stmt)) {
    handler_ptr->process(decl_ref_expr, env_ptr);

  } else if (clang::ConstantExpr* constant_expr = dyn_cast<clang::ConstantExpr>(stmt)) {
    recursive_visit(constant_expr->getSubExpr(), handler_ptr, env_ptr);

  } else if (clang::IntegerLiteral* integer_literal = dyn_cast<clang::IntegerLiteral>(stmt)) {
    handler_ptr->process(integer_literal, env_ptr);

  } else if (clang::CStyleCastExpr* c_style_cast_expr = dyn_cast<clang::CStyleCastExpr>(stmt)) {
    recursive_visit(c_style_cast_expr->getSubExpr(), handler_ptr, env_ptr);

  } else if (clang::CXXStaticCastExpr* cxx_static_cast_expr = dyn_cast<clang::CXXStaticCastExpr>(stmt)) {
    recursive_visit(cxx_static_cast_expr->getSubExpr(), handler_ptr, env_ptr);

  } else if (clang::SwitchStmt* switch_stmt = dyn_cast<clang::SwitchStmt>(stmt)) {
    recursive_visit(switch_stmt->getCond(), handler_ptr, env_ptr);
    recursive_visit(switch_stmt->getBody(), handler_ptr, env_ptr);

    handler_ptr->process(switch_stmt, env_ptr);

  } else if (clang::CaseStmt* case_stmt = dyn_cast<clang::CaseStmt>(stmt)) {
    env_ptr->update(case_stmt);
    recursive_visit(case_stmt->getLHS(), handler_ptr, env_ptr);
    recursive_visit(case_stmt->getRHS(), handler_ptr, env_ptr);
    recursive_visit(case_stmt->getSubStmt(), handler_ptr, env_ptr);

    handler_ptr->process(case_stmt, env_ptr);
    env_ptr->clear_switch_case_info();

  } else if (clang::ConditionalOperator* conditional_operator = dyn_cast<clang::ConditionalOperator>(stmt)) {
    recursive_visit(conditional_operator->getCond(), handler_ptr, env_ptr);
    recursive_visit(conditional_operator->getTrueExpr(), handler_ptr, env_ptr);
    recursive_visit(conditional_operator->getFalseExpr(), handler_ptr, env_ptr);

    handler_ptr->process(conditional_operator, env_ptr);

  } else if (clang::CXXFunctionalCastExpr* cxx_functional_cast_expr =
             dyn_cast<clang::CXXFunctionalCastExpr>(stmt)) {
    recursive_visit(cxx_functional_cast_expr->getSubExpr(), handler_ptr, env_ptr);

    handler_ptr->process(cxx_functional_cast_expr, env_ptr);

  } else if (clang::GNUNullExpr* gnu_null_expr = dyn_cast<clang::GNUNullExpr>(stmt)) {
    handler_ptr->process(gnu_null_expr, env_ptr);

  } else {
    LOG(INFO) << "unsupported stmt, trated as string: " << stmt_to_string(stmt);
  }
}

template<typename T, typename Handler>
void ExtractMethodVisitor::visit_loop(T* loop_stmt,
                                      Handler* handler_ptr,
                                      Env* env_ptr) {
  // 添加 loop_var
  visit_loop_init(loop_stmt, env_ptr);

  // loop init
  visit_loop_range(loop_stmt, handler_ptr, env_ptr);

  auto& loop_info = env_ptr->mutable_loop_info();
  if (loop_info) {
    loop_info->set_loop_state(LoopStage::BODY);
  }

  clang::Stmt* body = loop_stmt->getBody();

  for (auto body_it = body->child_begin(); body_it != body->child_end(); body_it++) {
    recursive_visit(*body_it, handler_ptr, env_ptr);
  }

  handler_ptr->process(loop_stmt, env_ptr);

  env_ptr->pop_loop_var();
}

template<typename T, typename Handler>
void ExtractMethodVisitor::visit_loop_range(T* loop_stmt,
                                           Handler* handler_ptr,
                                           Env* env_ptr) {
  for (auto it = loop_stmt->child_begin(); it != loop_stmt->child_end(); it++) {
    if (*it != nullptr) {
      if (clang::CompoundStmt* compound_stmt = dyn_cast<clang::CompoundStmt>(*it)) {
        continue;
      }

      recursive_visit(*it, handler_ptr, env_ptr);
    }
  }
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
