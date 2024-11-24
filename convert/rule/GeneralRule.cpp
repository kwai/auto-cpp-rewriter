#include <cstdio>
#include <regex>
#include <absl/strings/str_join.h>
#include <sstream>
#include "../Env.h"
#include "../Tool.h"
#include "../ExprInfo.h"
#include "../ExprParser.h"
#include "GeneralRule.h"
#include "../Deleter.h"
#include "../info/MethodInfo.h"
#include "../info/ActionMethodInfo.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

// 以下几种 if 条件需要直接修改代码:
// 1. pos < adlog.item_size(), 判断 pos 。
// 2. adlog.item_size() > pos, 所有逻辑都在 if body 里。
// 3. common info 的枚举判断，需要去掉 if 判断条件，将 if 里的内容作为替换后的代码。
// 4. action_detail 的 find 判断，需要改成 bs 的 if 判断, 需要从 Env 获取 bs 的 action_detail 变量。
//
// 然后再删掉多余的变量定义, 并添加新的需要的变量定义。
void GeneralRule::process(clang::IfStmt* if_stmt, Env* env_ptr) {
  process_deleted_var(env_ptr);

  const auto& if_info = env_ptr->cur_if_info();
  if (!if_info) {
    return;
  }

  if (env_ptr->parent() != nullptr && env_ptr->parent()->is_root() && env_ptr->is_first_if()) {
    env_ptr->parent()->set_first_if_stmt(if_stmt);
    env_ptr->parent()->set_is_first_if_check_item_pos_cond(if_info->is_check_item_pos_cond());

    if (if_info->is_check_item_pos_include()) {
      env_ptr->parent()->set_is_first_if_check_item_pos_include_cond(true);
    }
  }

  if (!if_stmt->hasElseStorage() && env_ptr->is_all_if_stmt_deleted()) {
    rewriter_.ReplaceText(if_stmt, "");
    return;
  }

  if (if_info->is_check_item_pos_include()) {
    rewriter_.ReplaceText(if_stmt, get_compound_stmt_rewritten_text(if_stmt->getThen()));
  }

  if (env_ptr->get_all_new_defs().size() > 0) {
    if (if_stmt->hasElseStorage()) {
      if (clang::IfStmt* else_if_stmt = dyn_cast<clang::IfStmt>(if_stmt->getElse())) {
        std::ostringstream oss;
        std::string s = rewriter_.getRewrittenText(find_source_range(if_stmt->getElse()));
        oss << "{\n    " << s << "\n}\n";
        rewriter_.ReplaceText(if_stmt->getElse(), oss.str());
      }
    }
  }

  // 必须放在最后，否则会 core
  rewriter_.InsertTextBefore(if_stmt, env_ptr->get_all_new_defs());
}

void GeneralRule::process(clang::ForStmt* for_stmt, Env* env_ptr) {
  process_loop(for_stmt, env_ptr);
}


void GeneralRule::process(clang::CXXForRangeStmt* cxx_for_range_stmt, Env* env_ptr) {
  process_loop(cxx_for_range_stmt, env_ptr);
}

void GeneralRule::process(clang::DeclStmt* decl_stmt, Env* env_ptr) {
  if (clang::VarDecl* var_decl = dyn_cast<clang::VarDecl>(decl_stmt->getSingleDecl())) {
    if (var_decl->hasInit()) {
      // 例: thread_local static std::vector<::auto_cpp_rewriter::CommonInfoAttr> action_list(user_attr_map_size);
      clang::Expr* init_expr = var_decl->getInit();
      std::string s = rewriter_.getRewrittenText(decl_stmt);

      // 替换变量声明中的 ad 枚举类型
      if (tool::is_ad_enum(init_expr->getType())) {
        static std::regex p_ad_enum("(::)?([^ ]+) ([^ ]+) =");
        s = std::regex_replace(s, p_ad_enum, std::string("::bs::") + "$2 $3 =");
        rewriter_.ReplaceText(decl_stmt, s);
      }

      // 替换 std::string 为 absl::string_view
      if (s.find("string") != std::string::npos) {
        s = tool::fix_std_string(s);

        if (clang::CXXConstructExpr* cxx_construct_expr = dyn_cast<clang::CXXConstructExpr>(init_expr)) {
          if (cxx_construct_expr->getNumArgs() > 0) {
            auto expr_info_ptr = parse_expr(cxx_construct_expr->getArg(0), env_ptr);
            if (expr_info_ptr->is_from_adlog()) {
              if (s.find("std::string") != std::string::npos) {
                rewriter_.ReplaceText(decl_stmt, tool::fix_string_view(s));
              }
            }
          }
        }
      }

      if (tool::is_common_info_vector(var_decl->getType())) {
        // common info 对应的类型确定后才能替换
        auto f_replace = [](Env* env_ptr, clang::Stmt* stmt) {
          static std::regex p("std::vector<.*>");

          if (clang::DeclStmt* decl_stmt = dyn_cast<clang::DeclStmt>(stmt)) {
            if (clang::VarDecl* var_decl = dyn_cast<clang::VarDecl>(decl_stmt->getSingleDecl())) {
              if (var_decl->hasInit()) {
                // 例: thread_local static std::vector<::auto_cpp_rewriter::CommonInfoAttr> action_list(user_attr_map_size);
                clang::Expr* init_expr = var_decl->getInit();
                auto expr_info_ptr = parse_expr(init_expr, env_ptr);

                std::string new_decl;
                if (env_ptr->is_combine_feature() && !expr_info_ptr->is_item_field()) {
                  new_decl = std::regex_replace(stmt_to_string(decl_stmt), p, "std::vector<BSRepeatedField<int64_t, true>>");
                } else {
                  new_decl = std::regex_replace(stmt_to_string(decl_stmt), p, "std::vector<BSRepeatedField<int64_t>>");
                }

                return StmtReplacement{stmt, new_decl};
              }
            }
          }

          return StmtReplacement{stmt, stmt_to_string(stmt)};
        };

        rewriter_.emplace_lazy_replace(env_ptr, decl_stmt, f_replace);
      }

      std::string stmt_str = rewriter_.getRewrittenText(decl_stmt);
      if (starts_with(stmt_str, "string ")) {
        static std::regex p_string("string ");
        std::string s = std::regex_replace(stmt_str, p_string, "std::string ");
        rewriter_.ReplaceText(decl_stmt, s);
      }
    }
  }

  std::string stmt_str = stmt_to_string(decl_stmt);
  if (starts_with(stmt_str, "vector")) {
    static std::regex p_vector("vector<(.*)>");
    static std::regex p_string("vector<string>");
    std::string s = std::regex_replace(stmt_str, p_vector, "std::vector<$1>");
    s = std::regex_replace(s, p_string, "vector<std::string>");
    LOG(INFO) << "replace decl_stmt: " << stmt_str << ", text: " << s;
    rewriter_.ReplaceText(decl_stmt, s);
  }
}

void GeneralRule::process(clang::CallExpr* call_expr, Env* env_ptr) {
  std::shared_ptr<ExprInfo> expr_info_ptr = parse_expr(call_expr, env_ptr);
  if (expr_info_ptr == nullptr) {
    LOG(INFO) << "expr_info_ptr is nullptr!";
  }

  // 示例:
  // teams/ad/ad_algorithm/feature/fast/impl/extract_callback_event_sparse.h
  // const google::protobuf::EnumDescriptor *callback_event_type_descriptor =
  //     auto_cpp_rewriter::AdCallbackLog::EventType_descriptor();
  // auto &ocpc_action_type_name = auto_cpp_rewriter::AdActionType_Name(
  // auto_cpp_rewriter::AdActionType(ocpc_action_type));
  // auto_cpp_rewriter::AdCallbackLog::EventType_Parse(ocpc_action_type_name,
  // &callback_event_)
  if (expr_info_ptr->is_enum_proto_call()) {
    rewriter_.ReplaceText(call_expr, std::string("::bs::") + rewriter_.getRewrittenText(call_expr));
    return;
  }

  // 在 SeqListRule 中处理。
  if (expr_info_ptr->is_seq_list_reco_proto_type()) {
    return;
  }

  if (expr_info_ptr->is_from_seq_list()) {
    return;
  }

  if (!expr_info_ptr->need_replace()) {
    return;
  }

  // 确定了 common info 的数据类型
  if (CommonAttrInfo::is_common_info_method(expr_info_ptr->callee_name())) {
    rewriter_.run_lazy_replace();
  }

  rewriter_.ReplaceText(call_expr, expr_info_ptr->get_bs_field_value());
}

void GeneralRule::process(clang::CXXMemberCallExpr* cxx_member_call_expr, Env* env_ptr) {
  std::shared_ptr<ExprInfo> expr_info_ptr = parse_expr(cxx_member_call_expr, env_ptr);
  if (expr_info_ptr == nullptr) {
    LOG(INFO) << "expr_info_ptr is nullptr!";
  }

  if (expr_info_ptr->callee_name() == "name_value") {
    return;
  }

  if (expr_info_ptr->callee_name() == "is_train") {
    rewriter_.ReplaceText(cxx_member_call_expr, "bslog.is_train()");
  }

  // 统一在 ActionDetailRule 中处理。
  if (expr_info_ptr->is_action_detail_leaf()) {
    return;
  }

  // 在 CommonInfoRule 中处理。
  if (expr_info_ptr->is_common_info_map_find_expr()) {
    return;
  }

  // 函数调用, 替换 action 参数
  // teams/ad/ad_algorithm/feature/fast/impl/extract_user_ad_action_cnt.h
  // add_feature(item_played5s_action_base_infos, item_click_action_base_infos,
  //             before_product_map, after_product_map, before_industry_map, after_industry_map,
  //             process_time);
  if (expr_info_ptr->is_cxx_member_call_expr() &&
      env_ptr->is_feature_other_method(expr_info_ptr->get_first_caller_name())) {
    if (const auto feature_info = env_ptr->get_feature_info()) {
      if (const MethodInfo* method_info_ptr =
          feature_info->find_method_info(expr_info_ptr->get_first_caller_name())) {
        for (size_t i = 0; i < cxx_member_call_expr->getNumArgs(); i++) {
          clang::Expr* arg = cxx_member_call_expr->getArg(i);
          std::string arg_str = stmt_to_string(arg);

          // add_feature action_detail param
          const NewActionParam& new_param = method_info_ptr->find_new_action_param(i);
          if (new_param.origin_name().size() > 0) {
            ExprInfo* call_expr_param = expr_info_ptr->call_expr_param(i);
            if (call_expr_param != nullptr) {
              std::string prefix = call_expr_param->get_bs_enum_str();
              std::vector<std::string> new_names = env_ptr->find_new_action_param_var_name(prefix, new_param);
              if (new_names.size() > 0) {
                rewriter_.ReplaceText(arg, absl::StrJoin(new_names, ","));
              } else {
                LOG(INFO) << "cannot find new_names for action param, prefix: " << prefix
                          << ", cxx_member_call_expr: " << expr_info_ptr->origin_expr_str()
                          << ", i: " << i
                          << ", arg: " << stmt_to_string(arg);
              }
            } else {
              LOG(INFO) << "something is wrong! call_expr_param is nullptr, i: " << i
                        << ", expr: " << expr_info_ptr->origin_expr_str();
            }
          }

          // GetSeqList
          if ( arg_str == "adlog" || arg_str == "ad_log") {
            rewriter_.ReplaceText(arg, "bslog");
          }
        }
      }
    }
  }

  // GetNormQuery 逻辑固定, 直接字符串替换
  // teams/ad/ad_algorithm/feature/fast/impl/extract_combine_query_search_pos.h
  // GetNormQuery(adlog, &query);
  if (expr_info_ptr->is_get_norm_query()) {
    rewriter_.ReplaceText(cxx_member_call_expr, replace_get_norm_query(cxx_member_call_expr));
    return;
  }

  // reco_user_info 字段需要分两次替换。通过 config->rewrite_reco_user_info 字段区分。
  //
  // 第一次只替换 adlog。如下所示
  // adlog.has_reco_user_info() => bslog.has_reco_user_info();
  // adlog.reco_user_info() => bslog.reco_user_info();
  //
  // 第二次替换为 bs 格式。
  const std::string bs_enum_str = expr_info_ptr->get_bs_enum_str();
  if (tool::is_str_from_reco_user_info(bs_enum_str) && env_ptr->is_in_loop_init()) {
    if (auto &loop_info = env_ptr->mutable_loop_info()) {
      loop_info->set_origin_size_var(expr_info_ptr->origin_expr_str());
    } else {
      LOG(ERROR) << "cannot find loop_info, decl ref: "
                 << expr_info_ptr->to_string();
    }
  }

  if (!expr_info_ptr->need_replace()) {
    return;
  }

  // 循环中 string loop var, 如 app_package.data(), app_package.size()
  if (expr_info_ptr->is_caller_str_ref() &&
      !expr_info_ptr->is_from_repeated_common_info() &&
      !expr_info_ptr->is_from_query_token()) {
    if (expr_info_ptr->is_from_adlog()) {
      if (auto expr_parent = expr_info_ptr->parent()) {
        std::string bs_enum_str = expr_parent->get_bs_enum_str();
        if (const absl::optional<NewVarDef>& var = env_ptr->find_new_def(bs_enum_str)) {
          std::ostringstream oss;
          if (env_ptr->is_loop_var(expr_parent->origin_expr_str())) {
            oss << var->name() << ".Get(idx)." << expr_info_ptr->callee_with_params(rewriter_);
          } else {
            oss << var->name() << "." << expr_info_ptr->callee_with_params(rewriter_);
          }

          LOG(INFO) << "cxx_member_call_expr: " << stmt_to_string(cxx_member_call_expr)
                    << ", replace: " << oss.str();
          rewriter_.ReplaceText(cxx_member_call_expr, oss.str());
        } else {
          LOG(INFO) << "cannot find new_var_def, bs_enum_str: " << bs_enum_str
                    << ", expr: " << expr_info_ptr->origin_expr_str();
        }
      }
    }
    return;
  }

  // 确定了 common info 的数据类型
  if (CommonAttrInfo::is_common_info_method(expr_info_ptr->callee_name())) {
    rewriter_.run_lazy_replace();
  }

  if (expr_info_ptr->is_from_action_detail_map() &&
      expr_info_ptr->contains_loop_var() &&
      expr_info_ptr->contains_template_parameter()) {
    if (const auto& action_detail_fixed_info = env_ptr->get_action_detail_fixed_info()) {
      if (absl::optional<std::string> field_name = expr_info_ptr->get_action_detail_field_name()) {
        rewriter_.ReplaceText(cxx_member_call_expr,
                              action_detail_fixed_info->get_bs_var_name(env_ptr, *field_name));
      }
    }
    return;
  }

  if (expr_info_ptr->is_parent_str_ref() || expr_info_ptr->is_parent_str_type()) {
    if (expr_info_ptr->parent() != nullptr) {
      std::ostringstream oss;
      std::string new_name = expr_info_ptr->parent()->get_bs_field_value();
      if (expr_info_ptr->callee_name() == "c_str") {
        oss << new_name << ".data()";
      } else {
        oss << new_name << "." << expr_info_ptr->callee_name() << "()";
      }
      LOG(INFO) << "repalce, expr: " << expr_info_ptr->origin_expr_str()
                << ", new_expr: " << oss.str();
      rewriter_.ReplaceText(cxx_member_call_expr, oss.str());
    }

    return;
  }

  std::string new_name = expr_info_ptr->get_bs_field_value();

  if (new_name.size() > 0) {
    LOG(INFO) << "repalce, expr: " << expr_info_ptr->origin_expr_str()
              << ", new_expr: " << new_name;
    rewriter_.ReplaceText(cxx_member_call_expr, new_name);
  }

  if (const auto& decl_info = env_ptr->cur_decl_info()) {
    if (new_name == decl_info->name()) {
      rewriter_.ReplaceText(decl_info->decl_stmt(), rm_decl_type(decl_info->decl_stmt()));
    }
  }
}

void GeneralRule::process(clang::MemberExpr* member_expr, Env* env_ptr) {
  std::shared_ptr<ExprInfo> expr_info_ptr = parse_expr(member_expr, env_ptr);
  if (!expr_info_ptr->is_from_adlog()) {
    return;
  }

  rewriter_.ReplaceText(member_expr, expr_info_ptr->get_bs_field_value());
}

void GeneralRule::process(clang::DeclRefExpr* decl_ref_expr, Env* env_ptr) {
  auto expr_info_ptr = parse_expr(decl_ref_expr, env_ptr);

  if (const auto& decl_info = env_ptr->cur_decl_info()) {
    if (expr_info_ptr->origin_expr_str() == decl_info->name()) {
      return;
    }
  }

  // 赋值操作左边的值不替换。
  if (const auto& binary_op_info = env_ptr->cur_binary_op_info()) {
    if (binary_op_info->is_assign_op()) {
      if (expr_info_ptr->origin_expr_str() == binary_op_info->left_expr_str()) {
        return;
      }
    }
  }

  if (expr_info_ptr->is_ad_enum()) {
    rewriter_.ReplaceText(decl_ref_expr, expr_info_ptr->get_bs_field_value());
    return;
  }

  if (expr_info_ptr->is_from_adlog()) {
    if (expr_info_ptr->is_basic()) {
      const std::string bs_enum_str = expr_info_ptr->get_bs_enum_str();
      if (tool::is_str_from_reco_user_info(bs_enum_str) && env_ptr->is_in_loop_init()) {
        if (auto& loop_info = env_ptr->mutable_loop_info()) {
          loop_info->set_origin_size_var(expr_info_ptr->origin_expr_str());
        } else {
          LOG(ERROR) << "cannot find loop_info, decl ref: " << expr_info_ptr->to_string();
        }
      } else {
        // common info loop var 在 common info 中处理。
        std::string new_str = expr_info_ptr->get_bs_field_value();
        if (new_str.size() > 0) {
          LOG(INFO) << "replace decl_ref_expr: " << stmt_to_string(decl_ref_expr) << ", text: " << new_str;
          rewriter_.ReplaceText(decl_ref_expr, new_str);
        }
      }
    }
  }
}

void GeneralRule::process(clang::CXXNullPtrLiteralExpr* cxx_null_ptr_literal_expr, Env* env_ptr) {
  auto expr_info_ptr = parse_expr(cxx_null_ptr_literal_expr, env_ptr);
}

void GeneralRule::process(clang::UnaryOperator* unary_operator, Env* env_ptr) {
  auto expr_info_ptr = parse_expr(unary_operator, env_ptr);
}

void GeneralRule::process(clang::BinaryOperator* binary_operator, Env* env_ptr) {
  if (const auto& binary_op_info = env_ptr->cur_binary_op_info()) {
    if (binary_op_info->is_assign_op()) {
      if (binary_op_info->left_expr_type() == ExprType::TEMPLATE_INT_REF &&
          binary_op_info->right_expr_type() == ExprType::INT) {
        if (auto feature_info = env_ptr->mutable_feature_info()) {
          feature_info->add_binary_op_stmt(binary_operator);
        }
      }
    } else if (binary_op_info->is_and_op()) {
      if (GlobalConfig::Instance()->rewrite_reco_user_info) {
        if (binary_op_info->left_expr_str() == "reco_user_info") {
          rewriter_.ReplaceText(binary_operator, rewriter_.getRewrittenText(binary_operator->getRHS()));
        } else if (binary_op_info->right_expr_str() == "reco_user_info") {
          rewriter_.ReplaceText(binary_operator, rewriter_.getRewrittenText(binary_operator->getLHS()));
        }
      }
    }
  }
}

void GeneralRule::process(clang::IntegerLiteral* integer_literal, Env* env_ptr) {
  auto expr_info_ptr = parse_expr(integer_literal, env_ptr);
  if (expr_info_ptr == nullptr) {
    return;
  }
}

void GeneralRule::process(clang::CXXOperatorCallExpr* cxx_operator_call_expr, Env* env_ptr) {
  LOG(INFO) << "cxx_operator_call_expr: " << stmt_to_string(cxx_operator_call_expr);
  auto expr_info_ptr = parse_expr(cxx_operator_call_expr, env_ptr);
  if (expr_info_ptr == nullptr) {
    return;
  }
}

void GeneralRule::process(clang::ReturnStmt *return_stmt, Env *env_ptr) {
  clang::Expr* ret_value = return_stmt->getRetValue();
  auto expr_info_ptr = parse_expr(ret_value, env_ptr);
  if (expr_info_ptr != nullptr) {
    if (auto feature_info = env_ptr->mutable_feature_info()) {
      MethodInfo& method_info = feature_info->touch_method_info(env_ptr->get_method_name());
      method_info.set_is_return_adlog_user_field(expr_info_ptr->is_adlog_user_field());
    }
  }
}

void GeneralRule::process(clang::CXXFunctionalCastExpr* cxx_functional_cast_expr, Env* env_ptr) {
  auto expr_info_ptr = parse_expr(cxx_functional_cast_expr, env_ptr);
  if (expr_info_ptr == nullptr) {
    return;
  }

  if (expr_info_ptr->is_enum_proto_call()) {
    rewriter_.ReplaceText(cxx_functional_cast_expr,
                          std::string("::bs::") + rewriter_.getRewrittenText(cxx_functional_cast_expr));
    return;
  }
}

void GeneralRule::process(clang::GNUNullExpr* gnu_null_expr, Env* env_ptr) {
  auto expr_info_ptr = parse_expr(gnu_null_expr, env_ptr);
  if (expr_info_ptr == nullptr) {
    return;
  }

  rewriter_.ReplaceText(gnu_null_expr, "nullptr");
}

void GeneralRule::process_loop_range(clang::ForStmt* for_stmt, Env* env_ptr, const std::string& body_text) {
}

void GeneralRule::process_loop_range(clang::CXXForRangeStmt* cxx_for_range_stmt,
                                     Env* env_ptr,
                                     const std::string& body_text) {
  if (env_ptr->new_defs().size() > 0) {
    std::ostringstream oss;
    std::string new_var_name = env_ptr->new_defs().begin()->first;
    oss << "  for (size_t " << env_ptr->get_last_loop_var() << " = 0; "
        << env_ptr->get_last_loop_var() << " < "
        << new_var_name << ".size(); "
        << env_ptr->get_last_loop_var() << "++) {\n  "
        << body_text << ";\n"
        << "}\n";

    rewriter_.ReplaceText(cxx_for_range_stmt, oss.str());
  }
}

// std::string product_name = item.ad_dsp_info().advertiser().base().product_name();
// product_name.find("xxx");
void GeneralRule::replace_decl_ref_var(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr == nullptr || env_ptr == nullptr) {
    return;
  }

  if (expr_info_ptr->is_decl_ref_expr()) {
    if (expr_info_ptr->is_from_adlog()) {
      std::string bs_enum_str = expr_info_ptr->get_bs_enum_str();
      if (const absl::optional<NewVarDef>& var = env_ptr->find_new_def(bs_enum_str)) {
        if (env_ptr->is_loop_var(expr_info_ptr->origin_expr_str())) {
          rewriter_.ReplaceText(expr_info_ptr->origin_expr(), var->name() + ".Get(idx)");
        } else {
          rewriter_.ReplaceText(expr_info_ptr->origin_expr(), var->name());
        }
      } else {
        LOG(INFO) << "cannot find new_var_def, bs_enum_str: " << bs_enum_str;
      }
    }

    return;
  }

  if (expr_info_ptr->parent() != nullptr) {
    replace_decl_ref_var(expr_info_ptr->parent().get(), env_ptr);
  }
}

std::string GeneralRule::replace_get_norm_query(clang::CXXMemberCallExpr* cxx_member_call_expr) {
  std::string s = stmt_to_string(cxx_member_call_expr);

  static std::regex p("this\\->GetNormQuery\\(adlog, \\&(\\w+)\\)");
  return std::regex_replace(s, p, "bs_util.BSGetNormQuery(bs, pos, &$1)");
}

std::string GeneralRule::rm_decl_type(clang::DeclStmt* decl_stmt) {
  std::string decl_stmt_str = rewriter_.getRewrittenText(decl_stmt);
  static std::regex p("([^ ]*) +([^ ]*) ?=");
  return std::regex_replace(decl_stmt_str, p, "$2 =");
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
