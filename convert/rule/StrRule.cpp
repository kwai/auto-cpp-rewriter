#include "../Env.h"
#include "../Tool.h"
#include "../ExprParser.h"
#include "../info/NewVarDef.h"
#include "StrRule.h"
#include <sstream>

namespace ks {
namespace ad_algorithm {
namespace convert {

void StrRule::process(clang::CallExpr *call_expr, Env *env_ptr) {
  auto expr_info_ptr = parse_expr(call_expr, env_ptr);
  if (expr_info_ptr == nullptr) {
    return;
  }


  // SplitString 参数需要从 absl::string_view 替换成 std::string
  // 示例: teams/ad/ad_algorithm/feature/fast/impl/extract_user_offline_retailer_keyword.h
  // base::SplitString(userAttr.string_value(), std::string(","), &userKeywordCnt);
  process_str_param_call(expr_info_ptr.get(), env_ptr);
}

void StrRule::process(clang::CXXMemberCallExpr* cxx_member_call_expr, Env* env_ptr) {
  auto expr_info_ptr = parse_expr(cxx_member_call_expr, env_ptr);
  if (expr_info_ptr == nullptr) {
    return;
  }

 // 示例: teams/ad/ad_algorithm/feature/fast/impl/extract_combine_region.h
  process_str_param_call(expr_info_ptr.get(), env_ptr);
}

void StrRule::process_str_param_call(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_call_expr()) {
    if (expr_info_ptr->callee_name() == "base::SplitString" ||
        expr_info_ptr->callee_name() == "base::StringToUint" ||
        expr_info_ptr->callee_name() == "GetRegion") {
      if (expr_info_ptr->call_expr_params_size() > 0) {
        auto param0 = expr_info_ptr->call_expr_param(0);
        std::ostringstream oss;
        if (param0 != nullptr && param0->is_string()) {
          std::string param0_text;
          if (param0->is_decl_ref_expr()) {
            param0_text = rewriter_.getRewrittenText(param0->origin_expr());
            if (param0_text.size() > 0) {
              oss << "std::string(" << param0_text << ".data(), " << param0_text << ".size())";
              rewriter_.ReplaceText(param0->origin_expr(), oss.str());
            }
          } else {
            param0_text = rewriter_.getRewrittenText(param0->expr());
            if (param0_text.size() > 0) {
              oss << "std::string(" << param0_text << ".data(), " << param0_text << ".size())";
              rewriter_.ReplaceText(param0->expr(), oss.str());
            }
          }

          LOG(INFO) << "replace str param call param0: " << param0->origin_expr_str()
                    << ", text: " << oss.str();
        }
      }
    }
  }
}

void StrRule::process(clang::BinaryOperator* binary_operator, Env *env_ptr) {
  // str 赋值, 并且右边不能是 str concat 这样的表达式。
  // 示例: teams/ad/ad_algorithm/feature/fast/impl/extract_combine_user_app_list_product.h
  // std::string product_name;
  // product_name = advertiser_base.product_name();
  if (binary_operator->isAssignmentOp()) {
    auto left_expr_info = parse_expr(binary_operator->getLHS(), env_ptr);
    auto right_expr_info = parse_expr(binary_operator->getRHS(), env_ptr);

    if (left_expr_info != nullptr && right_expr_info != nullptr) {
      if (!right_expr_info->is_cxx_operator_call_expr()) {
        clang::SourceRange source_range = find_source_range(binary_operator);
        process_str_assign(source_range, left_expr_info.get(), right_expr_info.get());
      }
    }
  }
}

void StrRule::process(clang::CXXOperatorCallExpr* cxx_operator_call_expr, Env *env_ptr) {
  // 注意，会用到 getRewrittenText，必须判断是否已经替换过，否则会 core。
  if (rewriter_.is_replace_visited(cxx_operator_call_expr)) {
    return;
  }

  auto expr_info_ptr = parse_expr(cxx_operator_call_expr, env_ptr);
  if (expr_info_ptr == nullptr) {
    return;
  }

  // adlog.context().app_id() + "_" => std::string(app_id) + "_"
  if (expr_info_ptr->is_str_concat()) {
    // LOG(INFO) << "cxx_operator_call_expr, before : " << rewriter_.getRewrittenText(cxx_operator_call_expr);
    if (expr_info_ptr->callee_name() == "operator+" && expr_info_ptr->call_expr_params_size() > 1) {
      std::string param0_str;
      std::string param1_str;

      if (auto param0 = expr_info_ptr->call_expr_param(0)) {
        if (param0->is_string() && param0->is_from_adlog() && param0->is_basic_scalar() &&
            (param0->is_decl_ref_expr() || param0->is_cxx_member_call_expr())) {
          std::string new_name = param0->get_bs_field_value();
          std::ostringstream oss_left;
          oss_left << "std::string(" << new_name << ".data(), " << new_name << ".size())";
          param0_str = oss_left.str();
        } else {
          param0_str = rewriter_.getRewrittenText(param0->expr());
        }
      }

      if (auto param1 = expr_info_ptr->call_expr_param(1)) {
        if (param1->is_string() && param1->is_from_adlog() && param1->is_basic_scalar() &&
            (param1->is_decl_ref_expr() || param1->is_cxx_member_call_expr())) {
          std::string new_name = param1->get_bs_field_value();
          std::ostringstream oss_right;
          oss_right << "std::string(" << new_name << ".data(), " << new_name << ".size())";
          param1_str = oss_right.str();
        } else {
          param1_str = rewriter_.getRewrittenText(param1->expr());
        }
      }

      std::ostringstream oss_text;
      oss_text << param0_str << " + " << param1_str;
      LOG(INFO) << "replace_cxx_operator_call_expr: " << stmt_to_string(cxx_operator_call_expr)
                << ", new_text: " << oss_text.str();
      rewriter_.ReplaceText(cxx_operator_call_expr, oss_text.str());
    }
    return;
  }

  // str 赋值, 并且右边不能是 str concat 这样的表达式。
  std::string op = stmt_to_string(cxx_operator_call_expr->getCallee());
  if (op == "operator=") {
    if (expr_info_ptr->call_expr_params_size() == 2) {
      auto left_expr_info = expr_info_ptr->call_expr_param(0);
      auto right_expr_info = expr_info_ptr->call_expr_param(1);

      if (left_expr_info != nullptr && right_expr_info != nullptr) {
        if (!right_expr_info->is_cxx_operator_call_expr()) {
          clang::SourceRange source_range = find_source_range(cxx_operator_call_expr);
          process_str_assign(source_range, left_expr_info, right_expr_info);
        }
      }
    }
  }
}

void StrRule::process_str_assign(clang::SourceRange source_range,
                                 ExprInfo* left_expr_info,
                                 ExprInfo* right_expr_info) {
  if (left_expr_info != nullptr && right_expr_info != nullptr) {
    if (left_expr_info->is_str_type() && right_expr_info->is_str_type()) {
      std::string param = right_expr_info->get_bs_field_value();
      std::ostringstream oss;
      oss << left_expr_info->origin_expr_str() << " = ";
      oss << "std::string(" << param << ".data(), " << param << ".size())";
      LOG(INFO) << "replace str assign, expr: " << rewriter_.getRewrittenText(source_range)
                << ", new text: " << oss.str();
      rewriter_.ReplaceText(source_range, oss.str());
    }
  }
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
