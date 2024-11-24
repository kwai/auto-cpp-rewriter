#include <regex>
#include <thread>
#include <algorithm>
#include <string>
#include <absl/strings/str_join.h>
#include <absl/strings/str_split.h>

#include "clang/AST/AST.h"
#include "clang/AST/ExprCXX.h"

#include "./Deleter.h"
#include "./ExprParser.h"
#include "./ExprParserDetail.h"
#include "./ExprParserBSField.h"
#include "./expr_parser/ExprParserQueryToken.h"
#include "./Tool.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

// 解析表达式。
std::shared_ptr<ExprInfo> parse_expr_simple(clang::Expr* expr, Env* env_ptr) {
  if (expr == nullptr) {
    LOG(INFO) << "expr is null";
    return nullptr;
  }

  if (clang::CXXMemberCallExpr* cxx_member_call_expr = dyn_cast<clang::CXXMemberCallExpr>(expr)) {
    auto expr_info_ptr = std::make_shared<ExprInfo>(expr, env_ptr);
    ExprInfo& expr_info = *expr_info_ptr;
    expr_info_ptr->set_raw_expr_str(stmt_to_string(expr));

    clang::Expr* caller = cxx_member_call_expr->getImplicitObjectArgument();
    if (clang::MemberExpr* callee = dyn_cast<clang::MemberExpr>(cxx_member_call_expr->getCallee())) {
      std::string callee_name = callee->getMemberDecl()->getNameAsString();

      expr_info.set_parent(std::move(parse_expr_simple(caller, env_ptr)));

      // for loop, begin is loop var
      if (callee_name == "begin") {
        return expr_info_ptr;
      }

      expr_info.set_callee_name(callee_name);
      expr_info.add_cur_expr_str(callee_name);

      for (unsigned i = 0; i < cxx_member_call_expr->getNumArgs(); i++) {
        expr_info.add_param(cxx_member_call_expr->getArg(i));
        auto param_expr_info_ptr = parse_expr_simple(cxx_member_call_expr->getArg(i), env_ptr);
        param_expr_info_ptr->set_caller_info(expr_info_ptr);
        expr_info_ptr->add_call_expr_param(std::move(param_expr_info_ptr));
      }

      return expr_info_ptr;
    } else {
      std::string expr_str = stmt_to_string(expr);
      LOG(INFO) << "unsupported cxx member call expr: " << expr_str;
      return expr_info_ptr;
    }
  } else if (clang::MemberExpr* member_expr = dyn_cast<clang::MemberExpr>(expr)) {
    clang::Expr* caller = member_expr->getBase();
    std::string member_str = tool::trim_this(member_expr->getMemberDecl()->getNameAsString());
    auto expr_info_ptr = std::make_shared<ExprInfo>(expr, env_ptr);
    expr_info_ptr->set_parent(std::move(parse_expr_simple(caller, env_ptr)));
    expr_info_ptr->set_callee_name(member_str);
    expr_info_ptr->set_raw_expr_str(stmt_to_string(expr));

    if (member_str != "first" && member_str != "second") {
      expr_info_ptr->add_cur_expr_str(member_str);
    }

    return expr_info_ptr;
  } else if (clang::ExprWithCleanups* expr_with_cleanups = dyn_cast<clang::ExprWithCleanups>(expr)) {
    return parse_expr_simple(expr_with_cleanups->getSubExpr(), env_ptr);
  } else if (clang::ImplicitCastExpr* cast_expr = dyn_cast<clang::ImplicitCastExpr>(expr)) {
    return parse_expr_simple(cast_expr->getSubExpr(), env_ptr);
  } else if (clang::MaterializeTemporaryExpr* material_expr = dyn_cast<clang::MaterializeTemporaryExpr>(expr)) {
    return parse_expr_simple(material_expr->getSubExpr(), env_ptr);
  } else if (clang::CXXBindTemporaryExpr* cxx_bind_temporary_expr = dyn_cast<clang::CXXBindTemporaryExpr>(expr)) {
    return parse_expr_simple(cxx_bind_temporary_expr->getSubExpr(), env_ptr);
  } else if (clang::ParenExpr* paren_expr = dyn_cast<clang::ParenExpr>(expr)) {
    return parse_expr_simple(paren_expr->getSubExpr(), env_ptr);
  } else if (clang::UnaryOperator* unary_operator = dyn_cast<clang::UnaryOperator>(expr)) {
    auto expr_info_ptr = std::make_shared<ExprInfo>(expr, env_ptr);
    expr_info_ptr->set_raw_expr_str(stmt_to_string(expr));
    auto op_str = clang::UnaryOperator::getOpcodeStr(unary_operator->getOpcode());
    std::string op(op_str.data(), op_str.size());
    expr_info_ptr->set_callee_name(op);
    expr_info_ptr->set_parent(std::move(parse_expr_simple(unary_operator->getSubExpr(), env_ptr)));

    auto param_expr_info = parse_expr_simple(unary_operator->getSubExpr(), env_ptr);
    expr_info_ptr->add_param(unary_operator->getSubExpr());
    param_expr_info->set_caller_info(expr_info_ptr);
    expr_info_ptr->add_call_expr_param(std::move(param_expr_info));

    return expr_info_ptr;
  } else if (clang::CXXOperatorCallExpr* cxx_operator_call_expr = dyn_cast<clang::CXXOperatorCallExpr>(expr)) {
    auto expr_info_ptr = std::make_shared<ExprInfo>(expr, env_ptr);
    expr_info_ptr->set_raw_expr_str(stmt_to_string(expr));

    std::string op = stmt_to_string(cxx_operator_call_expr->getCallee());
    expr_info_ptr->set_callee_name(op);
    expr_info_ptr->set_parent(std::move(parse_expr_simple(cxx_operator_call_expr->getArg(0), env_ptr)));

    for (size_t i = 0; i < cxx_operator_call_expr->getNumArgs(); i++) {
      auto param_expr_info_ptr = parse_expr_simple(cxx_operator_call_expr->getArg(i), env_ptr);
      param_expr_info_ptr->set_caller_info(expr_info_ptr);
      expr_info_ptr->add_call_expr_param(std::move(param_expr_info_ptr));
    }

    return expr_info_ptr;
  } else if (clang::BinaryOperator* binary_operator = dyn_cast<clang::BinaryOperator>(expr)) {
    auto expr_info_ptr = std::make_shared<ExprInfo>(expr, env_ptr);
    expr_info_ptr->set_raw_expr_str(stmt_to_string(expr));

    std::string op = binary_operator->getOpcodeStr().str();
    expr_info_ptr->set_callee_name(op);

    auto left = parse_expr_simple(binary_operator->getLHS(), env_ptr);
    left->set_caller_info(expr_info_ptr);
    expr_info_ptr->add_call_expr_param(std::move(left));

    auto right = parse_expr_simple(binary_operator->getRHS(), env_ptr);
    right->set_caller_info(expr_info_ptr);
    expr_info_ptr->add_call_expr_param(std::move(right));

    return expr_info_ptr;
  } else if (clang::CallExpr* call_expr = dyn_cast<clang::CallExpr>(expr)) {
    auto expr_info_ptr = std::make_shared<ExprInfo>(expr, env_ptr);
    expr_info_ptr->set_raw_expr_str(stmt_to_string(expr));
    expr_info_ptr->set_callee_name(tool::trim_this(stmt_to_string(call_expr->getCallee())));
    clang::Expr* caller = call_expr->getCallee();
    expr_info_ptr->set_parent(parse_expr_simple(caller, env_ptr));

    LOG(INFO) << "call_expr: " << stmt_to_string(call_expr)
              << ", caller: " << stmt_to_string(caller)
              << ", callee_name: " << expr_info_ptr->callee_name();
    for (size_t i = 0; i < call_expr->getNumArgs(); i++) {
      auto param_expr_info_ptr = parse_expr_simple(call_expr->getArg(i), env_ptr);
      param_expr_info_ptr->set_caller_info(expr_info_ptr);
      expr_info_ptr->add_call_expr_param(std::move(param_expr_info_ptr));
    }

    return expr_info_ptr;
  } else if (clang::CXXDependentScopeMemberExpr* cxx_dependent_scope_member_expr =
              dyn_cast<clang::CXXDependentScopeMemberExpr>(expr)) {
    auto expr_info_ptr = std::make_shared<ExprInfo>(expr, env_ptr);
    expr_info_ptr->set_callee_name(cxx_dependent_scope_member_expr->getMember().getAsString());
    clang::Expr* base = cxx_dependent_scope_member_expr->getBase();
    expr_info_ptr->set_parent(std::move(parse_expr_simple(base, env_ptr)));

    return expr_info_ptr;
  } else if (clang::ArraySubscriptExpr* array_subscript_expr = dyn_cast<clang::ArraySubscriptExpr>(expr)) {
    auto expr_info_ptr = std::make_shared<ExprInfo>(expr, env_ptr);
    expr_info_ptr->set_callee_name("[]");
    expr_info_ptr->set_parent(std::move(parse_expr_simple(array_subscript_expr->getBase(), env_ptr)));

    return expr_info_ptr;
  } else if (clang::CXXConstructExpr* cxx_construct_expr = dyn_cast<clang::CXXConstructExpr>(expr)) {
    // 正常只有一个参数
    if (cxx_construct_expr->getNumArgs() == 1) {
      auto expr_info_ptr = std::make_shared<ExprInfo>(expr, env_ptr);
      expr_info_ptr->set_parent(std::move(parse_expr_simple(cxx_construct_expr->getArg(0), env_ptr)));
      expr_info_ptr->set_raw_expr_str(stmt_to_string(expr));
      return expr_info_ptr;
    } else {
      auto expr_info_ptr = std::make_shared<ExprInfo>(expr, env_ptr);
      expr_info_ptr->set_raw_expr_str(stmt_to_string(expr));
      return expr_info_ptr;
    }
  } else if (clang::CXXFunctionalCastExpr* cxx_functional_cast_expr = dyn_cast<clang::CXXFunctionalCastExpr>(expr)) {
    auto expr_info_ptr = std::make_shared<ExprInfo>(expr, env_ptr);
    expr_info_ptr->set_raw_expr_str(stmt_to_string(expr));

    auto param_info_ptr = parse_expr_simple(cxx_functional_cast_expr->getSubExpr(), env_ptr);
    param_info_ptr->set_caller_info(expr_info_ptr);
    expr_info_ptr->add_call_expr_param(std::move(param_info_ptr));

    return expr_info_ptr;
  } else if (clang::ConstantExpr* constant_expr = dyn_cast<clang::ConstantExpr>(expr)) {
    return parse_expr_simple(constant_expr->getSubExpr(), env_ptr);
  } else if (clang::DeclRefExpr* decl_ref_expr = dyn_cast<clang::DeclRefExpr>(expr)) {
    LOG(INFO) << "parse decl_ref_expr: " << stmt_to_string(decl_ref_expr);
    auto expr_info_ptr = std::make_shared<ExprInfo>(expr, env_ptr);
    expr_info_ptr->set_origin_expr(decl_ref_expr);
    expr_info_ptr->set_raw_expr_str(stmt_to_string(expr));

    std::string expr_str = tool::trim_this(stmt_to_string(expr));

    if (expr_str == "pos") {
      return expr_info_ptr;
    }

    // 示例:
    // uint64_t min_time = UINT_MAX;
    // min_time = std::min(min_time, dsp_infos[i].action_timestamp());
    if (const auto& assign_info = env_ptr->cur_assign_info()) {
      if (expr_str == assign_info->name()) {
        return expr_info_ptr;
      }
    }

    clang::Expr* env_value = env_ptr->find(expr_str);

    // 互相引用的表达式这里判断不准
    if (env_ptr->is_decl_ref_contains_self(decl_ref_expr, env_value)) {
      return expr_info_ptr;
    }
    LOG(INFO) << "decl_ref_expr: " << stmt_to_string(decl_ref_expr)
              << ", env_value: " << stmt_to_string(env_value);

    if (env_value != nullptr) {
      auto expr_info_ptr = parse_expr_simple(env_value, env_ptr);
      if (expr_info_ptr != nullptr) {
        expr_info_ptr->set_origin_expr(decl_ref_expr);
        expr_info_ptr->set_raw_expr_str(stmt_to_string(expr));
      } else {
        LOG(INFO) << "parse decl ref expr failed, expr: " << stmt_to_string(expr);
      }

      return expr_info_ptr;
    }

    return expr_info_ptr;
  } else if (clang::IntegerLiteral* integer_literal = dyn_cast<clang::IntegerLiteral>(expr)) {
    auto expr_info_ptr = std::make_shared<ExprInfo>(expr, env_ptr);
    expr_info_ptr->set_raw_expr_str(stmt_to_string(expr));
    return expr_info_ptr;
  } else if (clang::CXXThisExpr* cxx_this_expr = dyn_cast<clang::CXXThisExpr>(expr)) {
    auto expr_info_ptr = std::make_shared<ExprInfo>(expr, env_ptr);
    expr_info_ptr->set_raw_expr_str(stmt_to_string(expr));
    return expr_info_ptr;
  } else if (clang::CXXNullPtrLiteralExpr* cxx_null_ptr_literal_expr =
             dyn_cast<clang::CXXNullPtrLiteralExpr>(expr)) {
    auto expr_info_ptr = std::make_shared<ExprInfo>(expr, env_ptr);
    expr_info_ptr->set_raw_expr_str(stmt_to_string(expr));
    return expr_info_ptr;
  } else if (clang::GNUNullExpr* gnu_null_expr = dyn_cast<clang::GNUNullExpr>(expr)) {
    auto expr_info_ptr = std::make_shared<ExprInfo>(expr, env_ptr);
    expr_info_ptr->set_raw_expr_str(stmt_to_string(expr));
    return expr_info_ptr;
  } else {
    LOG(INFO) << "unknown type, expr: " << stmt_to_string(expr);
    auto expr_info_ptr = std::make_shared<ExprInfo>(expr, env_ptr);
    expr_info_ptr->set_raw_expr_str(stmt_to_string(expr));
    return expr_info_ptr;
  }

  LOG(INFO) << "parse error, expr: " << stmt_to_string(expr);
  return nullptr;
}

std::shared_ptr<ExprInfo> parse_expr(clang::Expr* expr, Env* env_ptr) {
  auto expr_info_ptr = parse_expr_simple(expr, env_ptr);
  if (expr_info_ptr == nullptr) {
    LOG(INFO) << "parse expr error, return nullptr! expr: " << stmt_to_string(expr);
    return nullptr;
  }

  update_env_common_info(expr_info_ptr.get(), env_ptr);

  update_env_action_detail(expr_info_ptr.get(), env_ptr);
  update_env_action_detail_fixed(expr_info_ptr.get(), env_ptr);

  update_env_middle_node(expr_info_ptr.get(), env_ptr);
  update_env_double_list(expr_info_ptr.get(), env_ptr);
  update_env_get_seq_list(expr_info_ptr.get(), env_ptr);
  update_env_proto_list(expr_info_ptr.get(), env_ptr);
  update_env_query_token(expr_info_ptr.get(), env_ptr);

  update_env_general(expr_info_ptr.get(), env_ptr);

  update_env_bs_field(expr_info_ptr.get(), env_ptr);

  return expr_info_ptr;
}

void update_env_common_info(ExprInfo* expr_info_ptr, Env* env_ptr) {
  update_env_common_info_prepare(expr_info_ptr, env_ptr);
  update_env_common_info_normal(expr_info_ptr, env_ptr);
  update_env_common_info_fixed_list(expr_info_ptr, env_ptr);
  update_env_common_info_multi_map(expr_info_ptr, env_ptr);
  update_env_common_info_multi_int_list(expr_info_ptr, env_ptr);
}

void update_env_common_info_multi_map(ExprInfo* expr_info_ptr, Env* env_ptr) {
  // 多个 common info, 确定是 MULTI_MAP
  // user_attr_map_.find(user_attr.name_value())
  update_env_common_info_multi_map_touch(expr_info_ptr, env_ptr);
  update_env_common_info_multi_map_check_cond(expr_info_ptr, env_ptr);
}

void update_env_common_info_multi_int_list(ExprInfo *expr_info_ptr, Env *env_ptr) {
  update_env_common_info_multi_int_list_add_map(expr_info_ptr, env_ptr);
  update_env_common_info_multi_int_list_def(expr_info_ptr, env_ptr);
}

void update_env_action_detail(ExprInfo* expr_info_ptr, Env* env_ptr) {
  // 类似 common_info 的 list 和 map, 需要统一在 Env 中添加其定义。
  // const auto& ad_dsp_action_detail = adlog.user_info().user_real_time_action().real_time_dsp_action_detail();
  // auto iter = ad_dsp_action_detail.find(no);
  //
  // 对应变量的创建在 general 中进行。
  // 首次出现, 在 env_ptr 中创建 ActionDetailInfo。
  update_env_action_detail_prefix(expr_info_ptr, env_ptr);
  update_env_action_detail_new_def(expr_info_ptr, env_ptr);
  update_env_action_detail_check_cond(expr_info_ptr, env_ptr);
  update_env_action_detail_action_param_def(expr_info_ptr, env_ptr);
}


void update_env_action_detail_fixed(ExprInfo* expr_info_ptr, Env* env_ptr) {
  update_env_action_detail_fixed_touch(expr_info_ptr, env_ptr);
  update_env_action_detail_fixed_var_def(expr_info_ptr, env_ptr);
}

void update_env_middle_node(ExprInfo* expr_info_ptr, Env* env_ptr) {
  update_env_middle_node_root(expr_info_ptr, env_ptr);
  update_env_middle_node_leaf_def(expr_info_ptr, env_ptr);
  update_env_middle_node_check_cond(expr_info_ptr, env_ptr);
}

void update_env_get_seq_list(ExprInfo* expr_info_ptr, Env* env_ptr) {
  update_env_get_seq_list_touch(expr_info_ptr, env_ptr);
  update_env_get_seq_list_def(expr_info_ptr, env_ptr);
  update_env_get_seq_list_if_cond(expr_info_ptr, env_ptr);
  update_env_get_seq_list_loop(expr_info_ptr, env_ptr);
}

void update_env_proto_list(ExprInfo *expr_info_ptr, Env *env_ptr) {
  update_env_proto_list_leaf(expr_info_ptr, env_ptr);
  update_env_proto_list_size(expr_info_ptr, env_ptr);
}

void update_env_query_token(ExprInfo* expr_info_ptr, Env* env_ptr) {
  update_env_query_token_field_def(expr_info_ptr, env_ptr);
  update_env_query_token_loop(expr_info_ptr, env_ptr);
}

void update_env_general(ExprInfo* expr_info_ptr, Env* env_ptr) {
  update_env_general_iter_second(expr_info_ptr, env_ptr);
  update_env_general_basic_scalar_def(expr_info_ptr, env_ptr);
  update_env_general_str_call(expr_info_ptr, env_ptr);
  update_env_general_loop_var_method(expr_info_ptr, env_ptr);
  update_env_general_basic_expr(expr_info_ptr, env_ptr);
  update_env_general_binary_op_info(expr_info_ptr, env_ptr);
  update_env_general_decl_info(expr_info_ptr, env_ptr);
  update_env_general_get_norm_query(expr_info_ptr, env_ptr);
  update_env_general_loop_var_expr(expr_info_ptr, env_ptr);
  update_env_general_int_list_member_loop(expr_info_ptr, env_ptr);
  update_env_general_check_item_pos_include(expr_info_ptr, env_ptr);
  update_env_general_reco_user_info(expr_info_ptr, env_ptr);
  update_env_general_proto_map_loop(expr_info_ptr, env_ptr);
  update_env_general_proto_list_size_method(expr_info_ptr, env_ptr);
}

void update_env_bs_field(ExprInfo* expr_info_ptr, Env* env_ptr) {
  update_env_bs_field_decl(expr_info_ptr, env_ptr);
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
