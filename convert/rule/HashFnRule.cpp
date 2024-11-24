#include "../Env.h"
#include "../Tool.h"
#include "../ExprParser.h"
#include "../info/NewVarDef.h"
#include "HashFnRule.h"
#include <sstream>

namespace ks {
namespace ad_algorithm {
namespace convert {

void HashFnRule::process(clang::DeclStmt *decl_stmt, Env *env_ptr) {
  std::string stmt_str = stmt_to_string(decl_stmt);
  if (starts_with(stmt_str, "std::hash<std::string>")) {
    rewriter_.RemoveText(decl_stmt);
  }
}

void HashFnRule::process(clang::CXXOperatorCallExpr *cxx_operator_call_expr, Env *env_ptr) {
  auto expr_info_ptr = parse_expr(cxx_operator_call_expr, env_ptr);
  if (expr_info_ptr == nullptr) {
    return;
  }

  // 字符串 hash
  // hash_fn(product_name)
  if (expr_info_ptr->is_cxx_operator_call_expr()) {
    if (expr_info_ptr->callee_name() == "operator()") {
      if (expr_info_ptr->call_expr_params_size() == 2) {
        auto param0 = expr_info_ptr->call_expr_param(0);
        auto param1 = expr_info_ptr->call_expr_param(1);
        if (param0 != nullptr && param1 != nullptr) {
          LOG(INFO) << "cxx_operator_call_expr: " << stmt_to_string(cxx_operator_call_expr)
                    << ", param0: " << param0->origin_expr_str()
                    << ", param0 bs_text: " << param0->get_bs_field_value()
                    << ", param1: " << param1->origin_expr_str()
                    << ", param1 bs_text: " << param1->get_bs_field_value()
                    << ", param1 type_str: " << param1->expr()->getType().getAsString()
                    << ", param1 is_string: " << param1->is_string();
          if (param0->origin_expr_str() == "hash_fn" && param1->is_string()) {
            std::ostringstream oss;
            // std::string param1_text = param1->get_bs_field_value();
            std::string param1_text;
            if (param1->is_decl_ref_expr()) {
              param1_text = param1->origin_expr_str();
            } else {
              param1_text = rewriter_.getRewrittenText(param1->expr());
            }

            oss << "ad_nn::bs::Hash(" << param1_text << ")";
            rewriter_.ReplaceText(cxx_operator_call_expr, oss.str());

            if (auto feature_info = env_ptr->mutable_feature_info()) {
              feature_info->set_has_hash_fn_str(true);
            }
          }
        }
      }
    }
  }
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
