#include <regex>
#include <sstream>

#include "../Env.h"
#include "../Tool.h"
#include "../ExprInfo.h"
#include "../ExprParser.h"
#include "../info/ActionDetailInfo.h"
#include "ActionDetailRule.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

void ActionDetailRule::process(clang::IfStmt* if_stmt, Env* env_ptr) {
  if (const auto& if_info = env_ptr->cur_if_info()) {
    if (if_info->is_check_action_detail_cond()) {
      if (absl::optional<ActionDetailInfo>& action_detail_info = env_ptr->mutable_action_detail_info()) {
        std::ostringstream oss_action;
        std::string exists_expr = action_detail_info->get_exists_expr(env_ptr);
        if (if_info->is_check_equal()) {
          oss_action << "!" << exists_expr;
          rewriter_.ReplaceText(if_stmt->getCond(), oss_action.str());
          if (if_stmt->hasElseStorage()) {
            // 暂时不需要处理。
          } else {
            // 暂时不需要处理。
          }
        } else {
          oss_action << exists_expr;
          rewriter_.ReplaceText(if_stmt->getCond(), oss_action.str());
        }
      } else if (absl::optional<ActionDetailFixedInfo>& action_detail_fixed_info =
                 env_ptr->mutable_action_detail_fixed_info()) {
        std::ostringstream oss_action;
        std::string exists_expr = action_detail_fixed_info->get_action_detail_exists_expr(env_ptr);
        if (if_info->is_check_equal()) {
          oss_action << "if(!" << exists_expr << ") {\n"
                     << "        return;\n"
                     << "      }\n";
          rewriter_.ReplaceText(if_stmt, oss_action.str());
        } else {
          oss_action << exists_expr;
          rewriter_.ReplaceText(if_stmt->getCond(), oss_action.str());
        }
      } else {
        LOG(INFO) << "get action_detail_info error!";
      }
    }
  }
}

void ActionDetailRule::process(clang::ForStmt* for_stmt, Env* env_ptr) {
  // 多个 action_detail
  if (auto& loop_info = env_ptr->cur_mutable_loop_info()) {
    if (loop_info->is_int_list_member_loop() && loop_info->is_for_stmt()) {
      const std::vector<int>& int_list_member_values = loop_info->int_list_member_values();
      if (int_list_member_values.size() > 0) {
        std::string body_str = get_complete_rewritten_text(for_stmt->getBody(), env_ptr);
        clang::SourceRange source_range = find_source_range(for_stmt);
        process_action_detail_int_list(source_range, int_list_member_values, body_str, env_ptr);
      }
    }
  }
}

void ActionDetailRule::process(clang::CXXForRangeStmt* cxx_for_range_stmt, Env* env_ptr) {
  // 多个 action_detail
  if (auto& loop_info = env_ptr->cur_mutable_loop_info()) {
    if (!loop_info->is_for_stmt() && loop_info->is_int_list_member_loop()) {
      const std::vector<int>& int_list_member_values = loop_info->int_list_member_values();

      if (int_list_member_values.size() > 0) {
        clang::Stmt* body = cxx_for_range_stmt->getBody();
        std::string body_str = get_complete_rewritten_text(body, env_ptr);

        clang::SourceRange source_range = find_source_range(cxx_for_range_stmt);
        process_action_detail_int_list(source_range, int_list_member_values, body_str, env_ptr);
      }
    }
  }
}

void ActionDetailRule::process(clang::CXXMemberCallExpr *cxx_member_call_expr, Env *env_ptr) {
  auto expr_info_ptr = parse_expr(cxx_member_call_expr, env_ptr);
  if (expr_info_ptr == nullptr) {
    return;
  }

  if (!expr_info_ptr->is_action_detail_leaf()) {
    return;
  }

  if (auto parent = expr_info_ptr->parent()) {
    std::ostringstream oss;
    size_t param_size = parent->call_expr_params_size();
    if (param_size > 0) {
      auto param0 = parent->call_expr_param(0);
      if (param_size == 1) {
        if (param0 != nullptr) {
          oss << param0->get_bs_field_value();
        }
      } else {
        if (auto param1 = parent->call_expr_param(1)) {
          if (param1->is_binary_op_expr()) {
            if (param1->call_expr_params_size() == 2) {
              oss << param1->call_expr_param(0)->get_bs_field_value() << " "
                  << param1->callee_name() << " "
                  << param1->call_expr_param(1)->get_bs_field_value();
            } else {
              LOG(INFO)
                << "binary operator param size is not 2, binary operator: "
                << param0->origin_expr_str();
            }
          } else {
            oss << param1->get_bs_field_value();
          }
        }
      }
    } else {
      LOG(INFO) << "cannot find params, expr: "
                << expr_info_ptr->origin_expr_str();
    }

    std::string bs_text = expr_info_ptr->get_bs_field_value_action_detail_leaf(oss.str());
    LOG(INFO) << "bs_text: " << bs_text;
    rewriter_.ReplaceText(cxx_member_call_expr, bs_text);
  }
}

void ActionDetailRule::process_action_detail_int_list(clang::SourceRange source_range,
                                                      const std::vector<int>& int_list_member_values,
                                                      const std::string& body_str,
                                                      Env* env_ptr) {
  std::ostringstream oss;

  std::vector<std::string> new_bs_field_enums;

  std::regex p(std::string("_key_") + std::to_string(int_list_member_values[0]) + std::string("_"));
  for (size_t i = 0; i < int_list_member_values.size(); i++) {
    std::string new_str = std::string("_key_") + std::to_string(int_list_member_values[i]) + std::string("_");
    oss << "auto process_action_" << int_list_member_values[i] << " = [&]"
        << std::regex_replace(body_str, p, new_str) << ";\n\n    ";

    if (const auto ctor_info = env_ptr->get_constructor_info()) {
      const std::unordered_set<std::string>& bs_field_enums = ctor_info->bs_field_enums();
      for (const auto& x : bs_field_enums) {
        std::string new_bs_field_enum = std::regex_replace(x, p, new_str);
        if (new_bs_field_enum != x) {
          new_bs_field_enums.push_back(new_bs_field_enum);
        }
      }
    }
  }

  if (auto ctor_info = env_ptr->mutable_constructor_info()) {
    for (const auto& x : new_bs_field_enums) {
      ctor_info->add_bs_field_enum(x);
    }
  }

  for (size_t i = 0; i < int_list_member_values.size(); i++) {
    oss << "process_action_" << int_list_member_values[i] << "();\n    ";
  }

  rewriter_.ReplaceText(source_range, oss.str());
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
