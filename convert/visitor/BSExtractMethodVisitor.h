#pragma once

#include <nlohmann/json.hpp>
#include <absl/strings/match.h>

#include <string>
#include <vector>
#include <utility>
#include <memory>
#include <regex>

#include "clang/AST/ExprCXX.h"
#include "clang/AST/AST.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Basic/SourceLocation.h"

#include "../Env.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

using nlohmann::json;

class BSExtractMethodVisitor {
 protected:
  clang::Rewriter &rewriter_;

 public:
  explicit BSExtractMethodVisitor(clang::Rewriter &rewriter) : rewriter_(rewriter) {}     // NOLINT

  void visit_params(const clang::CXXMethodDecl *cxx_method_decl, Env *env_ptr);
  json visit(const clang::CXXMethodDecl *cxx_method_decl, FeatureInfo *feature_info_ptr);

  template <typename Handler>
  json recursive_visit(clang::Stmt *stmt, Handler *handler_ptr, Env *env_ptr);

  template <typename T, typename Handler>
  json visit_loop(T *loop_stmt, Handler *handler_ptr, Env *env_ptr);

  template <typename T, typename Handler>
  json visit_loop_range(T *loop_stmt, Handler *handler_ptr, Env *env_ptr);

  json visit_loop_init(clang::ForStmt *for_stmt, Env *env_ptr);

  json visit_loop_init(clang::CXXForRangeStmt *cxx_for_range_stmt,
                      Env *env_ptr);

  json visit_loop_var_decl(clang::VarDecl *var_decl, Env *env_ptr);

  void insert_new_def_at_begin(clang::Stmt* body, Env* env_ptr);
};

template<typename Handler>
json BSExtractMethodVisitor::recursive_visit(
  clang::Stmt *stmt,
  Handler* handler_ptr,
  Env* env_ptr
) {
  if (!stmt) {
    return nullptr;
  }

  if (clang::CompoundStmt* compound_stmt = dyn_cast<clang::CompoundStmt>(stmt)) {
    auto root = json::array({"do"});
    for (clang::CompoundStmt::body_iterator start = compound_stmt->body_begin();
         start != compound_stmt->body_end();
         start++) {
      root.push_back(recursive_visit(*start, handler_ptr, env_ptr));
    }

    return root;
  } else if (clang::DeclStmt* decl_stmt = dyn_cast<clang::DeclStmt>(stmt)) {
    auto root = json::array({"assign"});

    env_ptr->update(decl_stmt);
    if (clang::VarDecl* var_decl = dyn_cast<clang::VarDecl>(decl_stmt->getSingleDecl())) {
      root.push_back(var_decl->getNameAsString());

      if (var_decl->hasInit()) {
        root.push_back(recursive_visit(var_decl->getInit(), handler_ptr, env_ptr));
      }

      if (const clang::Expr* init_expr = var_decl->getAnyInitializer()) {
        root.push_back(recursive_visit(const_cast<clang::Expr*>(init_expr), handler_ptr, env_ptr));
      }
    }

    handler_ptr->process_to_json(decl_stmt, env_ptr);

    // decl_stmt 比较特殊, decl_info 只维持当前变量, 访问完当前 decl_stmt 后立即销毁。
    env_ptr->clear_decl_info();

    return root;
  } else if (clang::CXXConstructExpr* cxx_construct_expr = dyn_cast<clang::CXXConstructExpr>(stmt)) {
    auto root = json::array();
    for (size_t i = 0; i < cxx_construct_expr->getNumArgs(); i++) {
      root.push_back(recursive_visit(cxx_construct_expr->getArg(i), handler_ptr, env_ptr));
    }

    return root;
  } else if (clang::ExprWithCleanups* expr_with_cleanups = dyn_cast<clang::ExprWithCleanups>(stmt)) {
    return recursive_visit(expr_with_cleanups->getSubExpr(), handler_ptr, env_ptr);

  } else if (clang::ImplicitCastExpr* implicit_cast_expr = dyn_cast<clang::ImplicitCastExpr>(stmt)) {
    return recursive_visit(implicit_cast_expr->getSubExpr(), handler_ptr, env_ptr);

  } else if (clang::CXXBindTemporaryExpr* cxx_bind_temporary_expr =
             dyn_cast<clang::CXXBindTemporaryExpr>(stmt)) {
    return recursive_visit(cxx_bind_temporary_expr->getSubExpr(), handler_ptr, env_ptr);

  } else if (clang::CXXNullPtrLiteralExpr* cxx_null_ptr_literal_expr =
             dyn_cast<clang::CXXNullPtrLiteralExpr>(stmt)) {
    auto root = json::array({"value", "nullptr"});
    return root;
  } else if (clang::CXXMemberCallExpr* cxx_member_call_expr = dyn_cast<clang::CXXMemberCallExpr>(stmt)) {
    auto root = json::array({"call"});

    clang::Expr* caller = cxx_member_call_expr->getImplicitObjectArgument();
    if (clang::MemberExpr* callee = dyn_cast<clang::MemberExpr>(cxx_member_call_expr->getCallee())) {
      std::string callee_name = callee->getMemberDecl()->getNameAsString();
      root.push_back(callee_name);

      for (unsigned i = 0; i < cxx_member_call_expr->getNumArgs(); i++) {
        root.push_back(recursive_visit(cxx_member_call_expr->getArg(i), handler_ptr, env_ptr));
      }
    }

    handler_ptr->process_to_json(cxx_member_call_expr, env_ptr);

    return root;
  } else if (clang::CXXOperatorCallExpr* cxx_operator_call_expr =
             dyn_cast<clang::CXXOperatorCallExpr>(stmt)) {
    // 可能是 assign, compare, math
    env_ptr->update(cxx_operator_call_expr);

    std::string stmt_str = stmt_to_string(cxx_operator_call_expr);

    std::string op = stmt_to_string(cxx_operator_call_expr->getCallee());
    json root = json::array();
    static std::regex p("operator");

    if (op == "operator=") {
      // root = std::move(std::make_unique<AssignOp>());
    } else if (op == "operator==" || op == "operator<" || op == "operator>"
               || op == "operator<=" || op == "operator>=") {
      root = json::array({std::regex_replace(op, p, "")});
    } else if (op == "operator+" || op == "operator-" || op == "operator*" || op == "operator/") {
      root = json::array({std::regex_replace(op, p, "")});
    } else if (op == "operator()") {
      // 通过 CXXOperatorCallExpr 无法判断，只能通过字符串判断是否是 bs 中间节点函数。
      if (absl::StartsWith(stmt_str, "this->bs_util") || absl::StartsWith(stmt_str, "BS")) {
        static std::regex p_bs("(this->bs_util.)?BS(Get|Has)(.*)\\(");
        std::smatch m;

        if (std::regex_search(stmt_str, m, p_bs)) {
          // 最后一个 match 是模板类名
          if (m.size() > 0) {
            root = json::array({"bs_util", m[m.size() - 1]});
          }
        } else {
          LOG(ERROR) << "cannot find bs_util method, stmt_str: " << stmt_str;
        }
      } else {
        LOG(INFO) << "unsupported operator(): " << stmt_str;
      }
    } else {
      LOG(INFO) << "unsupported cxx_operator_call_expr: " << stmt_to_string(cxx_operator_call_expr)
                << ", op: " << op;
      return nullptr;
    }

    for (size_t i = 0; i < cxx_operator_call_expr->getNumArgs(); i++) {
      root.push_back(recursive_visit(cxx_operator_call_expr->getArg(i), handler_ptr, env_ptr));
    }

    env_ptr->clear_binary_op_info();
    return root;
  } else if (clang::CallExpr* call_expr = dyn_cast<clang::CallExpr>(stmt)) {
    // auto root = std::make_unique<CallOp>();
    // 会包含 CXXMemberCallExpr 和 CXXOperatorCallExpr, 此处逻辑需要再仔细考虑下
    auto root = json::array();

    if (clang::Expr* callee = call_expr->getCallee()) {
      root.push_back(tool::trim_this(stmt_to_string(callee)));
    }

    for (unsigned i = 0; i < call_expr->getNumArgs(); i++) {
      root.push_back(recursive_visit(call_expr->getArg(i), handler_ptr, env_ptr));
    }

    return root;
  } else if (clang::CXXDependentScopeMemberExpr *cxx_dependent_scope_member_expr =
           dyn_cast<clang::CXXDependentScopeMemberExpr>(stmt)) {
    return handler_ptr->process_to_json(cxx_dependent_scope_member_expr, env_ptr);

  } else if (clang::ArraySubscriptExpr* array_subscript_expr = dyn_cast<clang::ArraySubscriptExpr>(stmt)) {
    auto root = json::array();
    root.push_back(handler_ptr->process_to_json(array_subscript_expr, env_ptr));

    return root;
  } else if (clang::UnaryOperator *unary_operator = dyn_cast<clang::UnaryOperator>(stmt)) {
    auto op_str = clang::UnaryOperator::getOpcodeStr(unary_operator->getOpcode());
    auto root = json::array();
    root.push_back(recursive_visit(unary_operator->getSubExpr(), handler_ptr, env_ptr));

    return root;
  } else if (clang::MemberExpr* member_expr = dyn_cast<clang::MemberExpr>(stmt)) {
    auto root = json::array();
    root.push_back(handler_ptr->process_to_json(member_expr, env_ptr));

    return root;
  } else if (clang::BinaryOperator* binary_operator = dyn_cast<clang::BinaryOperator>(stmt)) {
    auto root = json::array();

    env_ptr->update(binary_operator);
    if (binary_operator->isAssignmentOp()) {
      env_ptr->update_assign_info(binary_operator);
      // root = std::move(std::make_unique<AssignOp>());
    } else {
      root.push_back({binary_operator->getOpcodeStr().str()});
    }

    root.push_back(recursive_visit(binary_operator->getLHS(), handler_ptr, env_ptr));

    root.push_back(recursive_visit(binary_operator->getRHS(), handler_ptr, env_ptr));

    env_ptr->clear_binary_op_info();
    if (binary_operator->isAssignmentOp()) {
      env_ptr->clear_assign_info();
    }

    return root;
  } else if (clang::ParenExpr* paren_expr = dyn_cast<clang::ParenExpr>(stmt)) {
    return recursive_visit(paren_expr->getSubExpr(), handler_ptr, env_ptr);

  } else if (clang::ImplicitCastExpr* implicit_cast_expr = dyn_cast<clang::ImplicitCastExpr>(stmt)) {
    return recursive_visit(implicit_cast_expr->getSubExpr(), handler_ptr, env_ptr);

  } else if (clang::MaterializeTemporaryExpr* materialize_temporary_expr =
             dyn_cast<clang::MaterializeTemporaryExpr>(stmt)) {
    return recursive_visit(materialize_temporary_expr->getSubExpr(), handler_ptr, env_ptr);

  } else if (clang::CXXThisExpr* cxx_this_expr = dyn_cast<clang::CXXThisExpr>(stmt)) {
    return handler_ptr->process_to_json(cxx_this_expr, env_ptr);

  } else if (clang::ReturnStmt* return_stmt = dyn_cast<clang::ReturnStmt>(stmt)) {
    return handler_ptr->process_to_json(return_stmt, env_ptr);

  } else if (clang::BreakStmt* break_stmt = dyn_cast<clang::BreakStmt>(stmt)) {
    return handler_ptr->process_to_json(break_stmt, env_ptr);

  } else if (clang::ContinueStmt* continue_stmt = dyn_cast<clang::ContinueStmt>(stmt)) {
    return handler_ptr->process_to_json(continue_stmt, env_ptr);

  } else if (clang::IfStmt* if_stmt = dyn_cast<clang::IfStmt>(stmt)) {
    Env new_env(env_ptr);
    new_env.update(if_stmt);

    auto root = json::array();

    clang::Expr* cond_expr = if_stmt->getCond();
    cond_expr = cond_expr->IgnoreImpCasts();
    root.push_back(recursive_visit(cond_expr, handler_ptr, &new_env));

    clang::Stmt* then_expr = if_stmt->getThen();
    clang::Stmt* else_expr = if_stmt->getElse();

    auto& if_info = new_env.cur_mutable_if_info();
    if_info->set_if_stage(IfStage::THEN);
    root.push_back(recursive_visit(then_expr, handler_ptr, &new_env));

    if_info->set_if_stage(IfStage::ELSE);
    root.push_back(recursive_visit(else_expr, handler_ptr, &new_env));

    if_info->set_if_stage(IfStage::END);

    return handler_ptr->process_to_json(if_stmt, &new_env);
  } else if (clang::ForStmt* for_stmt = dyn_cast<clang::ForStmt>(stmt)) {
    Env new_env(env_ptr);
    new_env.update(for_stmt);
    return visit_loop(for_stmt, handler_ptr, &new_env);

  } else if (clang::CXXForRangeStmt* cxx_for_range_stmt = dyn_cast<clang::CXXForRangeStmt>(stmt)) {
    Env new_env(env_ptr);
    new_env.update(cxx_for_range_stmt);

    return visit_loop(cxx_for_range_stmt, handler_ptr, &new_env);

  } else if (clang::DeclRefExpr* decl_ref_expr = dyn_cast<clang::DeclRefExpr>(stmt)) {
    return handler_ptr->process_to_json(decl_ref_expr, env_ptr);
  } else if (clang::ConstantExpr* constant_expr = dyn_cast<clang::ConstantExpr>(stmt)) {
    return recursive_visit(constant_expr->getSubExpr(), handler_ptr, env_ptr);

  } else if (clang::IntegerLiteral* integer_literal = dyn_cast<clang::IntegerLiteral>(stmt)) {
    auto root = json::array();

    return root;
  } else if (clang::CStyleCastExpr* c_style_cast_expr = dyn_cast<clang::CStyleCastExpr>(stmt)) {
    auto root = json::array();
    root.push_back(recursive_visit(c_style_cast_expr->getSubExpr(), handler_ptr, env_ptr));

    return root;
  } else if (clang::CXXStaticCastExpr* cxx_static_cast_expr = dyn_cast<clang::CXXStaticCastExpr>(stmt)) {
    auto root = json::array();
    root.push_back(recursive_visit(cxx_static_cast_expr->getSubExpr(), handler_ptr, env_ptr));

    return root;
  } else if (clang::SwitchStmt* switch_stmt = dyn_cast<clang::SwitchStmt>(stmt)) {
    auto root = json::array();

    root.push_back(recursive_visit(switch_stmt->getCond(), handler_ptr, env_ptr));
    root.push_back(recursive_visit(switch_stmt->getBody(), handler_ptr, env_ptr));

    return root;
  } else if (clang::CaseStmt* case_stmt = dyn_cast<clang::CaseStmt>(stmt)) {
    auto root = json::array();
    env_ptr->update(case_stmt);

    root.push_back(recursive_visit(case_stmt->getLHS(), handler_ptr, env_ptr));
    root.push_back(recursive_visit(case_stmt->getRHS(), handler_ptr, env_ptr));
    root.push_back(recursive_visit(case_stmt->getSubStmt(), handler_ptr, env_ptr));

    env_ptr->clear_switch_case_info();

    return root;
  } else if (clang::ConditionalOperator* conditional_operator = dyn_cast<clang::ConditionalOperator>(stmt)) {
    auto root = json::array();

    root.push_back(recursive_visit(conditional_operator->getCond(), handler_ptr, env_ptr));
    root.push_back(recursive_visit(conditional_operator->getTrueExpr(), handler_ptr, env_ptr));
    root.push_back(recursive_visit(conditional_operator->getFalseExpr(), handler_ptr, env_ptr));

    return root;
  } else if (clang::CXXFunctionalCastExpr* cxx_functional_cast_expr =
             dyn_cast<clang::CXXFunctionalCastExpr>(stmt)) {
    auto root = json::array();
    root.push_back(recursive_visit(cxx_functional_cast_expr->getSubExpr(), handler_ptr, env_ptr));

    return root;
  } else if (clang::GNUNullExpr* gnu_null_expr = dyn_cast<clang::GNUNullExpr>(stmt)) {
    auto root = json::array();

    return root;
  } else {
    LOG(INFO) << "unsupported stmt, trated as string: " << stmt_to_string(stmt);
    return nullptr;
  }
}

template<typename T, typename Handler>
json BSExtractMethodVisitor::visit_loop(T* loop_stmt,
                                      Handler* handler_ptr,
                                      Env* env_ptr) {
  // 添加 loop_var
  auto root = visit_loop_init(loop_stmt, env_ptr);
  if (root == nullptr) {
    return root;
  }

  // loop init
  root.push_back(visit_loop_range(loop_stmt, handler_ptr, env_ptr));

  auto& loop_info = env_ptr->mutable_loop_info();
  if (loop_info) {
    loop_info->set_loop_state(LoopStage::BODY);
  }

  clang::Stmt* body = loop_stmt->getBody();

  for (auto body_it = body->child_begin(); body_it != body->child_end(); body_it++) {
    root.push_back(recursive_visit(*body_it, handler_ptr, env_ptr));
  }

  handler_ptr->process_to_json(loop_stmt, env_ptr);

  env_ptr->pop_loop_var();
  return root;
}

template<typename T, typename Handler>
json BSExtractMethodVisitor::visit_loop_range(T* loop_stmt,
                                   Handler* handler_ptr,
                                   Env* env_ptr) {
  auto root = json::array();
  for (auto it = loop_stmt->child_begin(); it != loop_stmt->child_end(); it++) {
    if (*it != nullptr) {
      if (clang::CompoundStmt* compound_stmt = dyn_cast<clang::CompoundStmt>(*it)) {
        continue;
      }

      root.push_back(recursive_visit(*it, handler_ptr, env_ptr));
    }
  }

  return root;
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
