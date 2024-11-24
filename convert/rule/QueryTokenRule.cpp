#include <sstream>

#include "../Env.h"
#include "../ExprParser.h"
#include "../Tool.h"
#include "../info/NewVarDef.h"
#include "../handler/StrictRewriter.h"
#include "QueryTokenRule.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

void QueryTokenRule::process(clang::IfStmt* if_stmt, Env* env_ptr) {
}

void QueryTokenRule::process(clang::BinaryOperator* binary_operator, Env* env_ptr) {
  if (binary_operator == nullptr) {
    return;
  }

  std::string op = binary_operator->getOpcodeStr().str();
  clang::Expr *left_expr = binary_operator->getLHS();
  clang::Expr *right_expr = binary_operator->getRHS();

  if (binary_operator->isComparisonOp()) {
    auto left_info_ptr = parse_expr(left_expr, env_ptr);
    // 示例:
    // teams/ad/ad_algorithm/feature/fast/impl/extract_combine_querytoken_ad_campaign_id.cc
    // auto query_token = GetQueryToken(adlog);
    // if (query_token == nullptr || query_token->empty()) {
    //   return;
    // }
    if (left_info_ptr != nullptr &&
        left_info_ptr->is_decl_ref_expr() &&
        left_info_ptr->origin_expr() != nullptr &&
        left_info_ptr->is_proto_map_string_float_ref() &&
        !left_info_ptr->is_reco_proto_type()) {
      if (stmt_to_string(right_expr) == "nullptr") {
        if (op == "==") {
          rewriter_.ReplaceText(binary_operator, left_info_ptr->origin_expr_str() + ".is_empty()");
        } else if (op == "!=") {
          std::string s = std::string("!") + left_info_ptr->origin_expr_str() + ".is_empty()";
          rewriter_.ReplaceText(binary_operator, s);
        }
      }
    }
  }

  if (op == "||") {
    std::string left_str = rewriter_.getRewrittenText(left_expr);
    std::string right_str = rewriter_.getRewrittenText(right_expr);
    if (left_str == right_str) {
      rewriter_.ReplaceText(binary_operator, left_str);
    }
  }
}

void QueryTokenRule::process(clang::MemberExpr* member_expr, Env* env_ptr) {
}

void QueryTokenRule::process(clang::CXXMemberCallExpr *cxx_member_call_expr, Env *env_ptr) {
  auto expr_info_ptr = parse_expr(cxx_member_call_expr, env_ptr);
  if (expr_info_ptr == nullptr) {
    return;
  }

  if (!expr_info_ptr->is_from_query_token() || !expr_info_ptr->is_from_photo_text()) {
    return;
  }
}

void QueryTokenRule::process(clang::ForStmt *for_stmt, Env *env_ptr) {
  if (const auto &loop_info = env_ptr->cur_loop_info()) {
    if (loop_info->is_query_token_loop()) {
      std::string name = loop_info->loop_var_expr_str();
      std::ostringstream oss;
      oss << "for (size_t idx = 0; idx < " << name << ".size(); idx++) {\n    "
          << "auto query_key = " << name << ".GetKey(idx);\n"
          << get_compound_stmt_rewritten_text(for_stmt->getBody())
          << "\n}\n";

      rewriter_.ReplaceText(for_stmt, oss.str());
    }
  }
}

void QueryTokenRule::process(clang::CXXForRangeStmt *cxx_for_range_stmt, Env *env_ptr) {
}

void QueryTokenRule::process(clang::CXXOperatorCallExpr* cxx_operator_call_expr, Env* env_ptr) {
  // 示例: teams/ad/ad_algorithm/feature/fast/impl/extract_search_photo_asr_2.h
  // if (photo_asr_token->find(query_iter->first) == photo_asr_token->end()) {
  // ///
  // }
  std::string op = stmt_to_string(cxx_operator_call_expr->getCallee());
  if (op == "operator==" || op == "operator!=") {
    if (cxx_operator_call_expr->getNumArgs() == 2) {
      auto left_info_ptr = parse_expr(cxx_operator_call_expr->getArg(0), env_ptr);
      if (left_info_ptr != nullptr &&
          left_info_ptr->is_photo_text_find_expr() &&
          left_info_ptr->parent() != nullptr) {
        if (left_info_ptr->call_expr_params_size() == 1) {
          if (auto param = left_info_ptr->call_expr_param(0)) {
            std::string param_str = rewriter_.getRewrittenText(param->expr());
            std::string photo_text_map = left_info_ptr->parent()->origin_expr_str();
            if (photo_text_map.size() > 0 && param_str.size() > 0) {
              std::string bs_text = photo_text_map + ".Get(query_key).second";
              if (op == "operator==") {
                bs_text = std::string("!") + bs_text;
              }

              rewriter_.ReplaceText(cxx_operator_call_expr, bs_text);
            }
          }
        }
      }
    }
  }
}

} // namespace convert
} // namespace ad_algorithm
} // namespace ks
