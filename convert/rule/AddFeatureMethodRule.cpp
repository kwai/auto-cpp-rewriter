#include <regex>
#include <sstream>
#include <absl/strings/str_join.h>

#include "../Env.h"
#include "../Tool.h"
#include "../Deleter.h"
#include "../ExprInfo.h"
#include "../ExprParser.h"
#include "../info/ActionMethodInfo.h"
#include "../info/MethodInfo.h"
#include "AddFeatureMethodRule.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

void AddFeatureMethodRule::process(clang::CallExpr *call_expr, Env *env_ptr) {
  std::shared_ptr<ExprInfo> expr_info_ptr = parse_expr(call_expr, env_ptr);
  if (expr_info_ptr == nullptr) {
    LOG(INFO) << "expr_info_ptr is nullptr!";
  }

  // 见 docs/get_value_from_action.md。
  // 替换 add_feature 中调用 get_value_from_Action 的参数与名称。
  if (env_ptr->get_method_name() == "add_feature") {
    // ks::ad_algorithm::get_value_from_Action
    if (expr_info_ptr->callee_name() == ActionMethodInfo::name()) {
      if (auto feature_info = env_ptr->mutable_feature_info()) {

        size_t param_size = expr_info_ptr->call_expr_params_size();
        const ActionMethodInfo action_method_info(param_size);

        for (size_t i = 0; i < param_size; i++) {
          auto param_info_ptr = expr_info_ptr->call_expr_param(i);
          if (param_info_ptr != nullptr &&
              param_info_ptr->is_repeated_action_info()) {
            const NewActionParam &new_param = action_method_info.find_param(i);
            if (new_param.origin_name().size() > 0) {
              std::string new_str = new_param.get_bs_field_param_str();
              rewriter_.ReplaceText(param_info_ptr->expr(), new_str);
            } else {
              LOG(INFO) << "cannot find new action param for method: "
                        << action_method_info.name();
            }
          }
        }
      }

      std::string s = rewriter_.getRewrittenText(expr_info_ptr->expr());
      std::string new_s = std::regex_replace(
          s, std::regex("ks::ad_algorithm::get_value_from_Action"),
          "bs_get_value_from_action");
      rewriter_.ReplaceText(call_expr, new_s);
    }

    return;
  }
}

void AddFeatureMethodRule::process(clang::CXXMemberCallExpr* cxx_member_call_expr,
                                   Env* env_ptr) {
  std::shared_ptr<ExprInfo> expr_info_ptr = parse_expr(cxx_member_call_expr, env_ptr);
  if (expr_info_ptr == nullptr) {
    LOG(INFO) << "expr_info_ptr is nullptr!";
  }

  if (expr_info_ptr->is_parent_repeated_action_info()) {
    if (env_ptr->get_method_name() != "Extract") {
      if (expr_info_ptr->callee_name() == "size") {
        std::string name = expr_info_ptr->parent()->origin_expr_str() + std::string("_size");
        rewriter_.ReplaceText(cxx_member_call_expr, name);
      }

      return;
    }
  } else if (expr_info_ptr->is_parent_action_info()) {
    if (env_ptr->get_method_name() != "Extract") {
      std::regex p("(.*?)\\[(\\w+)\\]\\.(\\w+)\\(\\)");
      std::string name = std::regex_replace(expr_info_ptr->raw_expr_str(), p, "$1_$3");
      std::string new_text = std::regex_replace(expr_info_ptr->raw_expr_str(), p, "$1_$3.Get($2)");
      rewriter_.ReplaceText(cxx_member_call_expr, new_text);

      return;
    }
  }
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
