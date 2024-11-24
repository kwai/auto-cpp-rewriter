#include <regex>
#include <glog/logging.h>
#include <gflags/gflags.h>
#include "../Tool.h"
#include "../ExprInfo.h"
#include "../ExprParser.h"
#include "../info/MethodInfo.h"
#include "../info/ActionMethodInfo.h"
#include "../info/CommonInfoMultiIntList.h"
#include "OverviewHandler.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

void OverviewHandler::process(clang::CXXMemberCallExpr* cxx_member_call_expr, Env* env_ptr) {
  auto expr_info_ptr = parse_expr(cxx_member_call_expr, env_ptr);
  if (expr_info_ptr == nullptr) {
    return;
  }

  if (env_ptr->get_method_name() != "Extract") {
    LOG(INFO) << "expr: " << expr_info_ptr->origin_expr_str()
              << ", type: " << expr_info_ptr->expr()->getType().getAsString()
              << ", is_parent_repeated_action_info: " << expr_info_ptr->is_parent_repeated_action_info()
              << ", is_parent_action_info: " << expr_info_ptr->is_parent_action_info();
    if (auto feature_info = env_ptr->mutable_feature_info()) {
      MethodInfo& method_info = feature_info->touch_method_info(env_ptr->get_method_name());
      if (expr_info_ptr->is_parent_repeated_action_info()) {
        if (expr_info_ptr->callee_name() == "size") {
          std::string name = expr_info_ptr->parent()->origin_expr_str() + std::string("_size");
          method_info.add_new_action_field_param(expr_info_ptr->parent()->origin_expr_str(),
                                                 "size",
                                                 "int",
                                                 env_ptr->is_combine_feature(),
                                                 name);
          LOG(INFO) << "add_new_action_field_param in overview, origin_name: "
                    << expr_info_ptr->parent()->origin_expr_str()
                    << ", name: size, new_name: " << name;
        }
      } else if (expr_info_ptr->is_parent_action_info()) {
        std::regex p("(.*?)\\[(\\w+)\\]\\.(\\w+)\\(\\)");
        std::string origin_name = std::regex_replace(expr_info_ptr->raw_expr_str(), p, "$1");
        std::string name = std::regex_replace(expr_info_ptr->raw_expr_str(), p, "$1_$3");
        std::string new_text = std::regex_replace(expr_info_ptr->raw_expr_str(), p, "$1_$3.Get($2)");
        std::string type_str = tool::get_builtin_type_str(expr_info_ptr->expr()->getType());
        method_info.add_new_action_field_param(origin_name,
                                               expr_info_ptr->callee_name(),
                                               type_str,
                                               env_ptr->is_combine_feature(),
                                               name);
          LOG(INFO) << "add_new_action_field_param in overview, origin_name: " << origin_name
                    << ", name: " << name;
      }
    }
  }

  if (expr_info_ptr->is_repeated_common_info()) {
    if (auto feature_info = env_ptr->mutable_feature_info()) {
      if (const absl::optional<std::string>& prefix = env_ptr->get_common_info_prefix()) {
        auto &common_info_prepare = feature_info->mutable_common_info_prepare();
        if (!common_info_prepare) {
          common_info_prepare.emplace(*prefix);
        }
      }
    }
  }

  // 注册每个叶子节点 bs_enum_str 对应的 NewVarType。
  // common info 比较特殊。
  if (expr_info_ptr->is_from_adlog() &&
      expr_info_ptr->need_replace() &&
      expr_info_ptr->is_basic() &&
      !expr_info_ptr->is_repeated_common_info_size() &&
      !expr_info_ptr->is_from_repeated_common_info()) {
    if (auto feature_info = env_ptr->mutable_feature_info()) {
      NewVarType new_var_type = NewVarType::SCALAR;
      if (expr_info_ptr->is_from_list() || expr_info_ptr->is_common_info_list_method()) {
        new_var_type = NewVarType::LIST;
      } else if (expr_info_ptr->is_from_map() || expr_info_ptr->is_common_info_map_method()) {
        new_var_type = NewVarType::MAP;
      }

      std::string bs_enum_str = expr_info_ptr->get_bs_enum_str();
      if (tool::is_adlog_field(bs_enum_str)) {
        LOG(INFO) << "add_bs_enum_var_type: " << bs_enum_str
                  << ", expr: " << expr_info_ptr->origin_expr_str()
                  << ", new_var_type: " << static_cast<int>(new_var_type);
        feature_info->add_bs_enum_var_type(bs_enum_str, new_var_type);
      } else if (expr_info_ptr->is_from_middle_node()) {
        // 来自中间节点。
        if (!ends_with(bs_enum_str, "_size")) {
          std::string adlog_field = expr_info_ptr->get_adlog_field_str();
          if (expr_info_ptr->is_from_list()) {
            std::string inner_type = tool::get_builtin_type_str(expr_info_ptr->expr()->getType());
            LOG(INFO) << "add_middle_node_bs_enum_var_type: " << bs_enum_str
                      << ", list_inner_type: " << inner_type << ", expr: " << expr_info_ptr->origin_expr_str()
                      << ", new_var_type: " << static_cast<int>(new_var_type)
                      << ", adlog_field: " << adlog_field;
            feature_info->add_middle_node_bs_enum_var_type(
                bs_enum_str, new_var_type, adlog_field, inner_type);
          } else {
            LOG(INFO) << "add_middle_node_bs_enum_var_type: " << bs_enum_str
                      << ", scalar, expr: " << expr_info_ptr->origin_expr_str()
                      << ", new_var_type: " << static_cast<int>(new_var_type)
                      << ", adlog_field: " << adlog_field;
            feature_info->add_middle_node_bs_enum_var_type(bs_enum_str, new_var_type, adlog_field);
          }
        }
      }
    }
  }
}

void OverviewHandler::process(clang::CXXForRangeStmt* cxx_for_range_stmt, Env* env_ptr) {
}

void OverviewHandler::process(clang::CXXOperatorCallExpr* cxx_operator_call_expr, Env* env_ptr) {
  auto expr_info_ptr = parse_expr(cxx_operator_call_expr, env_ptr);
  if (expr_info_ptr == nullptr) {
    return;
  }


  if (expr_info_ptr->is_cxx_operator_call_expr()) {
    // teams/ad/ad_algorithm/feature/fast/impl/extract_user_rtl_photo_id_deep_action_90d.h
    // auto action_name = action_vec[i];
    // auto timestamp_name = timestamp_vec[i];
    // auto action_list = action_name2list[action_name];
    if (expr_info_ptr->callee_name() == "operator[]") {
      if (expr_info_ptr->call_expr_params_size() == 2) {
        auto param0_info_ptr = expr_info_ptr->call_expr_param(0);
        auto param1_info_ptr = expr_info_ptr->call_expr_param(1);
        if (param0_info_ptr != nullptr &&
            param1_info_ptr != nullptr &&
            param1_info_ptr->is_integral() &&
            param1_info_ptr->is_from_int_list_member()) {
          if (auto feature_info = env_ptr->mutable_feature_info()) {
            if (const auto& common_info_prepare = feature_info->common_info_prepare()) {
              if (const auto &common_info_prefix = common_info_prepare->prefix()) {
                if (auto &common_info_multi_int_list =
                    feature_info->touch_common_info_multi_int_list(*common_info_prefix)) {
                  if (param0_info_ptr->is_map_repeated_int_list_type() ||
                      param0_info_ptr->is_map_int_int_type()) {
                    std::string map_name = param0_info_ptr->origin_expr_str();
                    if (absl::optional<std::string> vec_name =
                            param1_info_ptr->find_int_list_member_name()) {
                      LOG(INFO) << "add_map_vec_connection: " << map_name
                                << ", " << *vec_name;
                      common_info_multi_int_list->add_map_vec_connection(map_name, *vec_name);
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

void OverviewHandler::process(clang::BinaryOperator *binary_operator, Env *env_ptr) {
  // 收集 common info 枚举或者模板参数
  auto expr_info_ptr = parse_expr(binary_operator, env_ptr);
  if (expr_info_ptr == nullptr) {
    return;
  }

  if (binary_operator->isComparisonOp()) {
    clang::Expr *left_expr = tool::get_inner_expr(binary_operator->getLHS());
    clang::Expr *right_expr = tool::get_inner_expr(binary_operator->getRHS());
    auto left_info_ptr = parse_expr(left_expr, env_ptr);
    auto right_info_ptr = parse_expr(right_expr, env_ptr);

    if (left_info_ptr != nullptr && right_info_ptr != nullptr) {
      if (auto feature_info = env_ptr->mutable_feature_info()) {
        if (auto& common_info_prepare = feature_info->mutable_common_info_prepare()) {
          if (left_info_ptr->is_name_value_method()) {
            if (right_info_ptr->is_template_int_ref()) {
              common_info_prepare->add_template_int_name(tool::trim_this(right_info_ptr->origin_expr_str()));
            } else if (right_info_ptr->is_common_info_enum_member_ref()) {
              common_info_prepare->add_template_int_name(tool::trim_this(right_info_ptr->origin_expr_str()));
            } else if (right_info_ptr->is_common_attr_info_enum()) {
              if (absl::optional<int> enum_value = right_info_ptr->get_common_attr_int_value()) {
                common_info_prepare->add_common_info_value(*enum_value);
              }
            } else if (right_info_ptr->is_integral()) {
              if (absl::optional<int> int_value = right_info_ptr->get_int_value()) {
                common_info_prepare->add_common_info_value(*int_value);
              }
            }
          }

          if (right_info_ptr->is_name_value_method()) {
            if (left_info_ptr->is_template_int_ref()) {
              common_info_prepare->add_template_int_name(left_info_ptr->origin_expr_str());
            } else if (left_info_ptr->is_common_info_enum_member_ref()) {
              common_info_prepare->add_template_int_name(tool::trim_this(left_info_ptr->origin_expr_str()));
            } else if (left_info_ptr->is_common_attr_info_enum()) {
              if (absl::optional<int> enum_value = left_info_ptr->get_common_attr_int_value()) {
                common_info_prepare->add_common_info_value(*enum_value);
              }
            } else if (left_info_ptr->is_integral()) {
              if (absl::optional<int> int_value = left_info_ptr->get_int_value()) {
                common_info_prepare->add_common_info_value(*int_value);
              }
            }
          }
        }
      }
    }
  }
}

void OverviewHandler::process(clang::CallExpr* call_expr, Env* env_ptr) {
  auto expr_info_ptr = parse_expr(call_expr, env_ptr);
  if (expr_info_ptr == nullptr) {
    return;
  }

  // 见 docs/get_value_from_action.md
  // 示例: teams/ad/ad_algorithm/feature/fast/impl/extract_user_ad_click_no_lps.h
  // add_feature 中会调用 get_value_from_Action 函数。需要将对应的参数添加到 add_feature 的 method_info 中。
  // get_value_from_Action 有两个重载版本，通过参数个数可以区分。
  //
  // inline bool add_feature(...) {
  //   ...
  //   ks::ad_algorithm::get_value_from_Action(
  //       item_played5s_action_list, item_click_action_list, period, process_time,
  //       &item_click_industry_vec, &item_click_product_vec, &product_result,
  //       &industry_result);
  //   ...
  // }
  if (env_ptr->get_method_name() == "add_feature") {
    if (expr_info_ptr->callee_name() == ActionMethodInfo::name()) {
      if (auto feature_info = env_ptr->mutable_feature_info()) {
        MethodInfo &method_info = feature_info->touch_method_info(env_ptr->get_method_name());

        size_t param_size = expr_info_ptr->call_expr_params_size();
        const ActionMethodInfo action_method_info(param_size);

        for (size_t i = 0; i < param_size; i++) {
          auto param_info_ptr = expr_info_ptr->call_expr_param(i);
          if (param_info_ptr != nullptr && param_info_ptr->is_repeated_action_info()) {
            const NewActionParam& new_param = action_method_info.find_param(i);
            if (new_param.origin_name().size() > 0) {
              method_info.add_new_action_param(i, new_param, param_info_ptr->origin_expr_str());
              LOG(INFO) << "add_new_action_param, i: " << i
                        << ", new_param: " << new_param.get_bs_field_param_str();
            } else {
              LOG(INFO) << "cannot find new action param for method: " << action_method_info.name();
            }
          }
        }
      }
    }
  }
}

void OverviewHandler::process(clang::DeclRefExpr* decl_ref_expr, Env* env_ptr) {
  if (decl_ref_expr == nullptr) {
    return;
  }

  if (decl_ref_expr->getDecl()) {
    if (decl_ref_expr->getDecl()->isTemplateParameter()) {
      if (auto feature_info = env_ptr->mutable_feature_info()) {
        feature_info->add_template_var_name(stmt_to_string(decl_ref_expr));
      }
    }
  }
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
