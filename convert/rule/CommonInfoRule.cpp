#include <regex>
#include <sstream>
#include "../Env.h"
#include "../Tool.h"
#include "../ExprInfo.h"
#include "../ExprParser.h"
#include "../info/CommonInfoMultiIntList.h"
#include "../handler/StrictRewriter.h"
#include "CommonInfoRule.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

void CommonInfoRule::process(clang::IfStmt* if_stmt, Env* env_ptr) {
  const auto& if_info = env_ptr->cur_if_info();
  if (!if_info) {
    return;
  }

  clang::Stmt* then_stmt = if_stmt->getThen();
  CommonInfoBodyText body_text;
  if (if_info->is_check_common_info_cond()) {
    body_text = get_common_info_body_text(if_stmt->getThen(), env_ptr);
  }

  // commmon info 中的非 loop break, 需要特殊处理。
  // 示例:  teams/ad/ad_algorithm/feature/fast/impl/extract_user_sjh_follow.h
  // for (const ::auto_cpp_rewriter::CommonInfoAttr& userAttr : adlog.user_info().common_info_attr()) {
  //   if (userAttr.name_value() == ::auto_cpp_rewriter::CommonInfoAttr_Name_USER_SJH_FOLLOW_LIST) {
  //     int times = userAttr.int_list_value_size() / 3;
  //     if (userAttr.int_list_value_size() % 3 != 0) {
  //       break;
  //     }
  //     ...
  //     break;
  //   }
  // }
  // 但是也有整体逻辑在 size_method 判断里面的，需要准确区分开。
  // 示例: teams/ad/ad_algorithm/feature/fast/impl/extract_combine_eds_comp_item_conv_list_ts.h
  // if (attr.name_value() == auto_cpp_rewriter::CommonInfoAttr_Name_COMPUTE_COMP_CONV_PRODUCT_NAME_LIST) {
  //   if (attr.int_list_value_size() > 0) {
  //     const auto& val = attr.int_list_value();
  //     int tcnt = 5;
  //     for (int i = 0; i < attr.int_list_value_size() && i < 50; i++) {
  //       if (attr.int_list_value(i) == prod_id) {
  //         if (i > ddeadline[tcnt]) {
  //           tcnt -= 1;
  //           if (tcnt < 0)
  //             break;
  //         }
  //         prod_cnts[tcnt] += 1;
  //       }
  //     }
  //   }
  // }
  // 整体是 if_stmt, 也包含 size_method, 但是不上上面的 userAttr.int_list_value_size() % 3 的情况。
  // 可能会有多个 common info detail, 因此也不能直接替换为 return。
  //
  // 按如下逻辑处理。
  // 1. 在 if cond 表达式中判断，如果是判断 expr != num 的形式，并且 expr 是 list_size_method 或者
  //    list_size_method % x 的形式，则标记 if 语句，并且在 common info detail 中记录 x 和 num 的值。
  // 2. 如果 if 的 body 是 break, 则将 if_stmt 整体删除。
  // 3. 在 common info detail 中进行替换的时候，将整体逻辑包含在 if (expr == num) 的语句中。
  if (if_info->is_check_common_info_list_size_not_equal() && if_info->is_body_only_break()) {
    rewriter_.ReplaceText(if_stmt, "");
  }

  if (if_info->is_check_common_info_map_end()) {
    if (if_info->is_check_not_equal()) {
      if (const auto& left_expr_str = if_info->left_expr_str()) {
        std::string new_text = *left_expr_str + ".second";
        rewriter_.ReplaceText(if_stmt->getCond(), new_text);
      }
    }
  }

  // 简单处理，假设逻辑都一样。
  if (auto& common_info_multi_map = env_ptr->mutable_common_info_multi_map()) {
    if (if_info->is_check_common_info_multi_cond()) {
      std::ostringstream oss_cond;
      oss_cond << "if (!" << common_info_multi_map->attr_name() << ".is_empty()) {\n"
               << "    " << body_text.get_bs_pre_text()
               << "    " << body_text.get_bs_loop_text()
               << "    " << body_text.get_bs_post_text()
               << "    }\n";

      rewriter_.ReplaceText(if_stmt, oss_cond.str());
    }

    // 替换构造函数里的 common info enum
    if (auto constructor_info = env_ptr->mutable_constructor_info()) {
      std::vector<CommonInfoEnumLoc>& common_info_enums = constructor_info->mutable_common_info_enums();
      for (size_t i = 0; i < common_info_enums.size(); i++) {
        absl::optional<int> enum_value_opt = find_common_attr_int_value(common_info_enums[i].enum_ref());
        if (enum_value_opt.has_value()) {
          std::string bs_enum_str = common_info_multi_map->prefix() + "_key_" + std::to_string(enum_value_opt.value());
          common_info_enums[i].set_bs_enum_str(bs_enum_str);
          std::ostringstream oss_enum_str;
          oss_enum_str << "static_cast<int>(BSFieldEnum::" << bs_enum_str
                       << ")";
          rewriter_.ReplaceText(common_info_enums[i].enum_ref(), oss_enum_str.str());
        } else {
          LOG(INFO) << "cannot find enum from expr: " << stmt_to_string(common_info_enums[i].enum_ref());
        }
      }
    }
  }

  // 在遍历 common info list 或者 map 的循环中, 如
  // for (int64 value : attr.int_list_value()) { ... }
  if (env_ptr->is_parent_common_info_loop()) {
    if (if_info->is_check_common_info_normal_cond()) {
        // common info if 判断
        // if (attr_name == auto_cpp_rewriter::CommonInfoAttr_NameExtendTwo_LSP_LATEST_LIVE_SEGMENT_INFO_TIMESTAMP) {
        //    ....
        //   continue;
        // }
      if (auto& common_info_normal = env_ptr->mutable_common_info_normal()) {
        if (auto& common_info_index = if_info->common_info_index()) {
          if (auto& common_info_detail = common_info_normal->last_mutable_common_info_detail()) {
            common_info_detail->set_bs_rewritten_text(body_text);
          }
        }
      }
    } else {
      // 先遍历 common info list 或者 map, 再判断枚举的情况。
      // 非 common info if。
      // for (int64 value : attr.int_list_value()) {
      //   if (x == 5) {
      //     ...
      //   }
      //   if (attr.name_value() ==
      //   auto_cpp_rewriter::CommonInfoAttr_NameExtendTwo_LSP_XXXXX) {
      //     ...
      //   }
      // }
      //
      // 需要将 if (x == 5) { ... } 保存起来。
      // 此种情况必须是遍历 list 或者 map 在外面，判断枚举是在里面，因此 loop 的 parent 必须是 common info
      // prefix 所对应的 loop, 而不能是 if。
      if (auto prepare_env = env_ptr->parent()->parent()) {
        if (const auto& switch_case_info = prepare_env->cur_switch_case_info()) {
          // 忽略
        } else if (prepare_env->is_loop()) {
          if (auto &common_info_prepare = prepare_env->cur_mutable_common_info_prepare()) {
            common_info_prepare->add_other_if_stmt_str(rewriter_.getRewrittenText(if_stmt));
          }
        }
      }
    }
  } else if (env_ptr->is_parent_loop()) {
    // 普通 common info 判断
    if (auto& common_info_normal = env_ptr->mutable_common_info_normal()) {
      if (if_info->is_check_common_info_normal_cond()) {
        if (auto& common_info_value = if_info->common_info_value()) {
          auto common_info_detail =
            common_info_normal->mutable_common_info_detail_by_value(*common_info_value);
          if (common_info_detail != nullptr && common_info_detail->bs_body_text() == "") {
            common_info_detail->set_bs_rewritten_text(body_text);
          }
        } else if (auto& common_info_index = if_info->common_info_index()) {
          auto& common_info_detail = common_info_normal->last_mutable_common_info_detail();
          common_info_detail->set_bs_rewritten_text(body_text);
        }
      }
    }

    if (auto& common_info_fixed_list = env_ptr->mutable_common_info_fixed_list()) {
      if (auto last_detail = common_info_fixed_list->last_mutable_common_info_detail()) {
        last_detail->set_bs_rewritten_text(body_text);
      }
    }
  }

  // if else 获取 common info value
  // 示例: teams/ad/ad_algorithm/feature/fast/impl/extract_universe_position_status.h
  // for (auto &attr : ad_log.context().info_common_attr()) {
  //   if (attr.name_value() == ::auto_cpp_rewriter::ContextInfoCommonAttr::AD_STYLE) {
  //     // AD_STYLE = 6, 中台广告位样式, 1: 信息流、2: 激励视频、3：插屏、4：开屏、5：banner
  //     AddFeature(GetFeature(FeaturePrefix::UNIVERSE_CONTEXT_AD_STYLE, attr.int_value()), 1.0, result);
  //   } else if (attr.name_value() == ::auto_cpp_rewriter::ContextInfoCommonAttr::COOPERATION_MODE) {
  //     // OOPERATION_MODE = 7;  // 中台对外合作模式，0：未知；2：API；3：H5
  //     AddFeature(GetFeature(FeaturePrefix::UNIVERSE_CONTEXT_COOPERATION_MODE, attr.int_value()),
  //                1.0, result);
  //   } else if (attr.name_value() == ::auto_cpp_rewriter::ContextInfoCommonAttr::MEDIUM_ATTRIBUTE) {
  //     // LOG_MEDIUM = 4 , 2: 站内流量, 4: 站外流量
  //     AddFeature(GetFeature(FeaturePrefix::CONTEXT_MEDIUM_ATTRIBUTE, attr.int_value()), 1.0, result);
  //   }
  // }
  if (if_info->is_check_common_info_normal_cond()) {
    if (auto& common_info_value = if_info->common_info_value()) {
      if (auto& common_info_normal = env_ptr->mutable_common_info_normal()) {
        auto common_info_detail =
          common_info_normal->mutable_common_info_detail_by_value(*common_info_value);
        if (common_info_detail != nullptr && common_info_detail->bs_body_text() == "") {
          common_info_detail->set_bs_rewritten_text(body_text);
        }
      }
    }
  }
}

void CommonInfoRule::process(clang::ForStmt* for_stmt, Env* env_ptr) {
}

// CommonInfoMultiIntList
void CommonInfoRule::process(clang::CXXForRangeStmt* cxx_for_range_stmt, Env* env_ptr) {
  if (const auto& common_info_multi_int_list = env_ptr->get_common_info_multi_int_list()) {
    if (const auto feature_info = env_ptr->get_feature_info()) {
      std::ostringstream oss;

      const std::unordered_map<std::string, std::string> &map_vec_connections =
        common_info_multi_int_list->map_vec_connections();

      const std::vector<std::string> &attr_map_names = common_info_multi_int_list->attr_map_names();
      for (size_t i = 0; i < attr_map_names.size(); i++) {
        auto it = map_vec_connections.find(attr_map_names[i]);
        if (it != map_vec_connections.end()) {
          const std::string vec_name = it->second;
          const std::vector<int> &int_values = feature_info->get_int_list_member_values(vec_name);
          if (int_values.size() > 0) {
            for (int v : int_values) {
              oss << attr_map_names[i] << "[" << v << "] = std::move("
                  << common_info_multi_int_list->get_functor_name(v) << "(bs, pos));\n";
            }
          } else {
            LOG(INFO) << "cannot find int_values from feature_info, vec_name: " << vec_name;
          }
        } else {
          LOG(INFO) << "cannot find vec_name in map_vec_connections, attr_map_name: " << attr_map_names[i];
        }
      }

      const std::vector<std::string> &attr_size_map_names = common_info_multi_int_list->attr_size_map_names();
      for (size_t i = 0; i < attr_size_map_names.size(); i++) {
        auto it = map_vec_connections.find(attr_size_map_names[i]);
        if (it != map_vec_connections.end()) {
          const std::string vec_name = it->second;
          const std::vector<int> &int_values = feature_info->get_int_list_member_values(vec_name);
          const std::string list_map_name =
            common_info_multi_int_list->find_correspond_list_map(attr_size_map_names[i]);
          if (list_map_name.size() > 0) {
            if (int_values.size() > 0) {
              for (int v : int_values) {
                oss << attr_size_map_names[i] << "[" << v << "] = "
                    << list_map_name << "[" << v << "]" << ".size();\n    ";
              }
            } else {
              LOG(INFO)
                  << "cannot find int_values from feature_info, vec_name: "
                  << vec_name;
            }
          } else {
            LOG(INFO) << "cannot find list_map_name, attr_size_map_name: " << attr_size_map_names[i];
          }
        } else {
          LOG(INFO)
              << "cannot find vec_name in map_vec_connections, attr_size_map_name: "
              << attr_size_map_names[i];
        }
      }

      rewriter_.ReplaceText(cxx_for_range_stmt, oss.str());
    }
  }

  if (env_ptr->get_method_name() == "helper") {
    if (const auto& loop_info = env_ptr->cur_loop_info()) {
      if (loop_info->is_common_info_list_map()) {
        const auto& loop_var = loop_info->loop_var();
        if (loop_var.size() > 0) {
          if (const auto &common_info_prepare = env_ptr->get_common_info_prepare()) {
            if (const auto &attr_name = common_info_prepare->attr_name()) {
              std::ostringstream oss;
              oss << "for (size_t idx = 0; idx < " << *attr_name << ".size(); idx++) {\n    "
                  << " auto " << loop_var << " = " << *attr_name << ".Get(idx);\n    "
                  << get_compound_stmt_rewritten_text(cxx_for_range_stmt->getBody())
                  << "\n}\n    ";

              rewriter_.ReplaceText(cxx_for_range_stmt, oss.str());
            }
          }
        } else {
          LOG(INFO) << "cannot find loop_var!";
        }
      }
    }
  }
}

void CommonInfoRule::process(clang::SwitchStmt* switch_stmt, Env* env_ptr) {
  if (const auto& common_info_normal = env_ptr->get_common_info_normal()) {
    env_ptr->update_template_common_info_values();
    std::ostringstream oss_res;

    const std::vector<std::shared_ptr<CommonInfoLeaf>>& common_info_details =
      common_info_normal->common_info_details();

    for (size_t i = 0; i < common_info_details.size(); i++) {
      oss_res << common_info_normal->get_bs_rewritten(&rewriter_, i)
              << "\n\n";
    }

    rewriter_.ReplaceText(switch_stmt, oss_res.str());
  }
}

void CommonInfoRule::process(clang::CaseStmt* case_stmt, Env* env_ptr) {
  if (const auto& switch_case_info = env_ptr->get_switch_case_info()) {
    if (auto& common_info_normal = env_ptr->mutable_common_info_normal()) {
      if (auto& common_info_index = switch_case_info->common_info_index()) {
        auto& common_info_detail = common_info_normal->last_mutable_common_info_detail();

        CommonInfoBodyText body_text = get_common_info_body_text(case_stmt->getSubStmt(), env_ptr);
        common_info_detail->set_bs_rewritten_text(body_text);

        std::string s = body_text.bs_body_text();

        // 处理 case 共用逻辑的情况
        // teams/ad/ad_algorithm/feature/fast/impl/extract_search_query_combine_match_num.cc
        // switch (photoAttr.name_value()) {
        // case ::auto_cpp_rewriter::CommonInfoAttr_NameExtendOne_SEARCH_ADS_PARSER_DESCRIPTION:
        // case ::auto_cpp_rewriter::CommonInfoAttr_NameExtendOne_SEARCH_ADS_PARSER_PRODUCT_NAME:
        // case ::auto_cpp_rewriter::CommonInfoAttr_NameExtendOne_SEARCH_ADS_PARSER_COVER_INFO:
        // case ::auto_cpp_rewriter::CommonInfoAttr_NameExtendOne_SEARCH_ADS_PARSER_CATEGORY_TAG:
        // case ::auto_cpp_rewriter::CommonInfoAttr_NameExtendOne_SEARCH_ADS_PARSER_PHOTO_OCR:
        // case ::auto_cpp_rewriter::CommonInfoAttr_NameExtendOne_SEARCH_ADS_PARSER_TEXT_ALL_V1:
        //   for (int i = 0; i < photoAttr.int_list_value_size() && i < 5; i++) {
        //     uint64_t des = static_cast<uint64_t>(photoAttr.int_list_value(i));
        //     if (rec.find(des) == rec.end()) {
        //       rec.insert(des);
        //     }
        //   }
        //   break;
        if (s.size() > 0) {
          for (int i = common_info_normal->common_info_details_size() - 2; i >= 0; i--) {
            auto& detail = common_info_normal->mutable_common_info_detail(i);

            if (detail->bs_body_text().size() > 0) {
              break;
            }

            detail->copy_except_int_value(common_info_detail.get());
            env_ptr->add_common_info_detail_def(*detail);
          }
        }
      }
    }
  }
}

void CommonInfoRule::process(clang::CXXOperatorCallExpr* cxx_operator_call_expr, Env* env_ptr) {
  auto expr_info_ptr = parse_expr(cxx_operator_call_expr, env_ptr);
  if (expr_info_ptr == nullptr) {
    return;
  }
}

void CommonInfoRule::process(clang::DeclStmt* decl_stmt, Env* env_ptr) {
  if (clang::VarDecl *var_decl = dyn_cast<clang::VarDecl>(decl_stmt->getSingleDecl())) {
    // teams/ad/ad_algorithm/feature/fast/impl/extract_user_rtl_product_name_shallow_action_7d.h
    // std::unordered_map<int, const google::protobuf::RepeatedField<::google::protobuf::int64> *>
    //   action_name2list;
    //
    // 简单处理，都当做 user int list 处理。
    if (tool::is_map_repeated_int_list_type(var_decl->getType())) {
      std::string var_name = var_decl->getNameAsString();

      std::ostringstream oss;
      oss << "std::unordered_map<int, BSRepeatedField<int64_t>> " << var_name << ";\n";

      rewriter_.ReplaceText(decl_stmt, oss.str());
    }

    // teams/ad/ad_algorithm/feature/fast/impl/extract_user_rtl_product_name_shallow_action_7d.h
    // auto action_list = action_name2list[action_name];
    if (tool::is_repeated_proto_ptr(var_decl->getType())) {
      static std::regex p("auto ");
      rewriter_.ReplaceText(decl_stmt, std::regex_replace(stmt_to_string(decl_stmt), p, "auto&"));
    }

    if (auto& common_info_prepare = env_ptr->cur_mutable_common_info_prepare()) {
      if (const auto& loop_info = env_ptr->cur_loop_info()) {
        if (loop_info->loop_stage() == LoopStage::BODY) {
          // 去掉 CommonInfoAttr 变量
          // const ::auto_cpp_rewriter::CommonInfoAttr & itemAttr = live_info->common_info_attr(i);
          if (!tool::is_common_info_enum(var_decl->getType())) {
            common_info_prepare->add_other_decl_stmt_str(stmt_to_string(decl_stmt));
          }
        }
      }
    }
  }
}

void CommonInfoRule::process(clang::CXXMemberCallExpr* cxx_member_call_expr, Env* env_ptr) {
  auto expr_info_ptr = parse_expr(cxx_member_call_expr, env_ptr);
  if (expr_info_ptr == nullptr) {
    return;
  }

  if (expr_info_ptr->parent() != nullptr &&
      expr_info_ptr->parent()->is_repeated_proto_ptr()) {
    std::string parent_str = expr_info_ptr->parent()->origin_expr_str();
    std::regex p(parent_str + "\\->");
    std::string s = std::regex_replace(expr_info_ptr->origin_expr_str(), p, parent_str + ".");
    rewriter_.ReplaceText(cxx_member_call_expr, s);
  }

  if (expr_info_ptr->callee_name() == "helper") {
    if (expr_info_ptr->call_expr_params_size() > 2) {
      ExprInfo *param1 = expr_info_ptr->call_expr_param(1);
      if (param1 != nullptr) {
        if (param1->is_common_info_struct_type()) {
          if (const auto &common_info_normal = env_ptr->get_common_info_normal()) {
            if (auto last_detail = common_info_normal->last_common_info_detail()) {
              std::string var_name = env_ptr->find_new_var_name(last_detail->get_bs_enum_str());
              if (var_name.size() > 0) {
                rewriter_.ReplaceText(param1->origin_expr(), var_name);
              } else {
                LOG(INFO) << "cannot find var_name in env_ptr for common info "
                             "helper, bs_enum_str: "
                          << last_detail->get_bs_enum_str();
              }
            }
          }
        }
      }
    }
  }

  if (expr_info_ptr->is_common_info_scalar_method() ||
      expr_info_ptr->is_common_info_list_method()) {
    if (const auto& common_info_fixed_list = env_ptr->get_common_info_fixed_list()) {
      if (auto last_detail = common_info_fixed_list->last_common_info_detail()) {
        rewriter_.ReplaceText(cxx_member_call_expr, last_detail->get_bs_var_name(env_ptr));
      }
    } else if (const auto &common_info_normal = env_ptr->get_common_info_normal()) {
      if (auto last_detail = common_info_normal->last_common_info_detail()) {
        rewriter_.ReplaceText(cxx_member_call_expr, last_detail->get_bs_var_name(env_ptr));
      }
    }
  }

  // 示例: ad/ad_algorithm/feature/fast/impl/extract_combine_realtime_action_match_cnt_v2.h
  // auto id = action_list[key_idx].int_list_value(i);
  if (expr_info_ptr->is_common_info_list_method()) {
    if (auto feature_info = env_ptr->get_feature_info()) {
      if (feature_info->has_common_info_multi_map()) {
        if (expr_info_ptr->parent() != nullptr && expr_info_ptr->call_expr_params_size() == 1) {
          if (auto param = expr_info_ptr->call_expr_param(0)) {
            std::string param_text = rewriter_.getRewrittenText(param);
            std::string text = expr_info_ptr->parent()->origin_expr_str() + ".Get(" + param_text + ")";
            LOG(INFO) << "replace common info multi map list value, expr: "
                      << expr_info_ptr->origin_expr_str()
                      << ", text: " << text;
            rewriter_.ReplaceText(cxx_member_call_expr, text);
          }
        }
      }
    }
  }

  if (expr_info_ptr->is_common_info_map_find_expr()) {
    if (const auto& common_info_normal = env_ptr->get_common_info_normal()) {
      if (const auto last_detail = common_info_normal->last_common_info_detail()) {
        std::string var_name = last_detail->get_bs_var_def_name(env_ptr);
        if (var_name.size() > 0) {
          if (expr_info_ptr->call_expr_params_size() == 1) {
            if (auto param = expr_info_ptr->call_expr_param(0)) {
              std::ostringstream oss;
              oss << var_name << ".Get(" << param->origin_expr_str() << ")";
              LOG(INFO) << "replace common info map find expr: " << expr_info_ptr->origin_expr_str()
                        << ", text: " << oss.str();
              rewriter_.ReplaceText(cxx_member_call_expr, oss.str());
            }
          }
        }
      }
    }
  }

  if (expr_info_ptr->is_common_info_list_size_method()) {
    if (const auto& common_info_normal = env_ptr->get_common_info_normal()) {
      if (auto last_detail = common_info_normal->last_common_info_detail()) {
        std::string var_name = last_detail->get_bs_var_def_name(env_ptr);
        if (var_name.size() > 0) {
          LOG(INFO) << "replace cxx_member_call_expr: " << expr_info_ptr->origin_expr_str()
                    << ", text: " << var_name + ".size()";
          rewriter_.ReplaceText(cxx_member_call_expr, var_name + ".size()");
        }
      }
    } else if (const auto& common_info_fixed_list = env_ptr->get_common_info_fixed_list()) {
      if (auto last_detail = common_info_fixed_list->last_common_info_detail()) {
        std::string var_name = last_detail->get_bs_var_def_name(env_ptr);
        if (var_name.size() > 0) {
          LOG(INFO) << "replace cxx_member_call_expr: " << expr_info_ptr->origin_expr_str()
                    << ", text: " << var_name + ".size()";
          rewriter_.ReplaceText(cxx_member_call_expr, var_name + ".size()");
        }
      }
    } else {
      if (const auto feature_info = env_ptr->get_feature_info()) {
        if (feature_info->has_common_info_multi_map()) {
          if (expr_info_ptr->parent() != nullptr) {
            std::string text = expr_info_ptr->parent()->origin_expr_str() + ".size()";
            LOG(INFO) << "replace cxx_member_call_expr: " << expr_info_ptr->origin_expr_str()
                      << ", text: " << text;
            rewriter_.ReplaceText(cxx_member_call_expr, text);
          }
        }
      }
    }
  }

  // 示例: teams/ad/ad_algorithm/feature/fast/impl/extract_creative_support_tag.h
  // int max_num = attr.string_list_value().size() > 20 ? 20 : attr.string_list_value().size();
  if (expr_info_ptr->callee_name() == "size") {
    if (auto parent = expr_info_ptr->parent()) {
      if (parent->is_common_info_list_method()) {
        std::string bs_text = parent->get_bs_field_value();
        if (bs_text.size() > 0) {
          rewriter_.ReplaceText(cxx_member_call_expr, bs_text + ".size()");
        }
      }
    }
  }
}

void CommonInfoRule::process(clang::CallExpr* call_expr, Env* env_ptr) {
  auto expr_info_ptr = parse_expr(call_expr, env_ptr);
  if (expr_info_ptr == nullptr) {
    return;
  }

  if (expr_info_ptr->callee_name() == "helper") {
    if (expr_info_ptr->call_expr_params_size() > 2) {
      ExprInfo *param1 = expr_info_ptr->call_expr_param(1);
      if (param1 != nullptr) {
        if (param1->is_common_info_struct_type()) {
          if (const auto &common_info_normal = env_ptr->get_common_info_normal()) {
            if (auto last_detail = common_info_normal->last_common_info_detail()) {
              std::string var_name = env_ptr->find_new_var_name(last_detail->get_bs_enum_str());
              if (var_name.size() > 0) {
                rewriter_.ReplaceText(param1->origin_expr(), var_name);
              } else {
                LOG(INFO) << "cannot find var_name in env_ptr for common info "
                             "helper, bs_enum_str: "
                          << last_detail->get_bs_enum_str();
              }
            }
          }
        }
      }
    }
  }
}

void CommonInfoRule::process(clang::BinaryOperator* binary_operator, Env* env_ptr) {
  auto expr_info_ptr = parse_expr(binary_operator, env_ptr);
}

CommonInfoBodyText CommonInfoRule::get_common_info_body_text(clang::Stmt* stmt, Env* env_ptr) {
  // 区分 common info loop 前后的语句，如果没有 common info
  // loop，则都作为之前的语句。 loop 以被替换为 body 语句。
  //
  // infer ItemFilter 中以下语句需要删掉，改写的 bs 代码中不需要此逻辑:
  // 1. uint64_t name_value = attr.name_value();
  // 2. if (attr.int_list_value().empty()) {
  //      break;
  //    }
  //
  // 示例: teams/ad/ad_nn/utils/item_filter.h
  // InnerLoopUnionHighCostFilterWeakenRoas
  //
  // for (const auto& attr : item.ad_dsp_info().common_info_attr()) {
  //   uint64_t name_value = attr.name_value();
  //   if (name_value != ::auto_cpp_rewriter::CommonInfoAttr_Name_ECOM_REALTIME_DIRECT_CREATIVE_RECENT_COST) {
  //     continue;
  //   }
  //   if (attr.int_list_value().empty()) {
  //     break;
  //   }
  //   for (auto val : attr.int_list_value()) { cost_total += val; }
  // }
  std::ostringstream oss_pre;
  std::ostringstream oss_loop_common_info;
  std::ostringstream oss_post;

  absl::optional<std::string> loop_name;

  if (clang::CompoundStmt* compound_stmt = dyn_cast<clang::CompoundStmt>(stmt)) {
    for (auto it = compound_stmt->child_begin(); it != compound_stmt->child_end();
         it++) {
      if (clang::BreakStmt *break_stmt = dyn_cast<clang::BreakStmt>(*it)) {
        continue;
      }

      if (clang::ContinueStmt *continue_stmt = dyn_cast<clang::ContinueStmt>(*it)) {
        continue;
      }

      // 去掉
      // uint64_t name_value = attr.name_value();
      if (clang::DeclStmt* decl_stmt = dyn_cast<clang::DeclStmt>(*it)) {
        if (clang::VarDecl* var_decl = dyn_cast<clang::VarDecl>(decl_stmt->getSingleDecl())) {
          if (var_decl->getNameAsString() == "name_value") {
            continue;
          }
        }
      }

      // 去掉
      // if (attr.int_list_value().empty()) {
      //   break;
      // }
      if (clang::IfStmt* if_stmt = dyn_cast<clang::IfStmt>(*it)) {
        clang::Expr* cond_expr = if_stmt->getCond();
        auto expr_info_ptr = parse_expr(cond_expr, env_ptr);
        if (expr_info_ptr != nullptr && expr_info_ptr->is_common_info_empty_method()) {
          continue;
        }
      }

      if (clang::DeclStmt* decl_stmt = dyn_cast<clang::DeclStmt>(*it)) {
        if (clang::VarDecl *var_decl = dyn_cast<clang::VarDecl>(decl_stmt->getSingleDecl())) {
          if (var_decl->hasInit()) {
            auto expr_info_ptr = parse_expr(var_decl->getInit(), env_ptr);

            // 忽略 int_list_value() 变量
            // const auto& values = itemAttr.int_list_value();
            //
            // 忽略 map 变量
            // 示例: teams/ad/ad_algorithm/feature/fast/impl/extract_combine_user_applist_cate_match_num.h
            // auto& install_app_cate_map = userAttr.map_int64_int64_value();
            // for (auto iter = install_app_cate_map.begin(); iter != install_app_cate_map.end(); iter++) {
            //   install_app_cate_map_fix[iter->first] = iter->second;
            // }
            //
            // 示例: teams/ad/ad_algorithm/feature/fast/impl/extract_fanstop_ocpx_action_type.h
            // const auto& bid_ocpc_type_map = attr.map_int64_int64_value();
            if (expr_info_ptr->is_common_info_list_method() || expr_info_ptr->is_common_info_map_method()) {
              loop_name.emplace(var_decl->getNameAsString());
              continue;
            }
          }
        }
      }

      std::string new_text = rewriter_.getRewrittenText(*it);
      new_text = fix_semicolon(new_text) + "\n";

      std::string stmt_str = stmt_to_string(*it);

      if (loop_name) {
        if (CommonAttrInfo::is_common_info_list_or_map_loop(stmt_str, *loop_name)) {
          oss_loop_common_info << new_text;
          continue;
        }
      }

      // 通过字符串判断
      if (tool::is_common_info_list_or_map_loop_stmt(*it)) {
        oss_loop_common_info << new_text;
      } else if (oss_loop_common_info.str().size() == 0) {
        oss_pre << new_text;
      } else {
        oss_post << new_text;
      }
    }
  } else {
    std::string new_text = fix_semicolon(rewriter_.getRewrittenText(stmt)) + "\n";

    if (tool::is_common_info_list_or_map_loop_stmt(stmt)) {
      oss_loop_common_info << new_text;
    } else {
      oss_pre << new_text;
    }
  }

  CommonInfoBodyText body_text;
  body_text.set_bs_rewritten_text(oss_pre.str(), oss_loop_common_info.str(), oss_post.str());

  return body_text;
}

} // convert
}  // namespace ad_algorithm
}  // namespace ks
