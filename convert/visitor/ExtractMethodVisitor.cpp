#include <ostream>
#include <regex>
#include <glog/logging.h>
#include <absl/strings/str_join.h>
#include <absl/strings/str_split.h>
#include <sstream>
#include "../Env.h"
#include "../Tool.h"
#include "../handler/OverviewHandler.h"
#include "../handler/AdlogFieldHandler.h"
#include "ExtractMethodVisitor.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

void ExtractMethodVisitor::visit(const clang::CXXMethodDecl* cxx_method_decl, FeatureInfo* feature_info_ptr) {
  auto config = GlobalConfig::Instance();

  std::string method_name = cxx_method_decl->getNameAsString();

  if (config->dump_ast) {
    LOG(INFO) << "dump method with body: " << method_name;
    cxx_method_decl->dump();

    if (cxx_method_decl->getBody() != nullptr) {
      cxx_method_decl->getBody()->dump();
    }
  }

  if (starts_with(method_name, "~") ||
      starts_with(method_name, "operator") ||
      method_name == feature_info_ptr->feature_name()) {
    return;
  }

  StrictRewriter strict_rewriter(rewriter_);
  // LazyReplace 里会保存 Env*, 所以 Handler 必须是局部变量，否则会 core
  AdlogFieldHandler adlog_field_handler(rewriter_);
  FeatureInfo& feature_info = *feature_info_ptr;

  Env env;
  ConstructorInfo& constructor_info = feature_info.mutable_constructor_info();
  env.set_method_name(method_name);

  get_overview_info(feature_info_ptr, cxx_method_decl, method_name);

  if (const auto& int_list_info = feature_info_ptr->common_info_multi_int_list()) {
    const auto& map_vec_connections = int_list_info->map_vec_connections();
    LOG(INFO) << "after overview map_vec_connections size: " << map_vec_connections.size();
  }

  env.set_feature_info(feature_info_ptr);
  env.add_template_var_names(feature_info_ptr->template_var_names());
  env.set_constructor_info(&constructor_info);

  LOG(INFO) << "========================= start process method, name: " << method_name
            << " ==================================";
  clang::Stmt *body = cxx_method_decl->getBody();

  std::string origin_body_str = strict_rewriter.getRewrittenText(body);

  if (config->use_reco_user_info) {
    config->rewrite_reco_user_info = false;
    recursive_visit (body, &adlog_field_handler, &env);

    // 将新增变量加到 body 最开始。
    std::string body_str = strict_rewriter.getRewrittenText(body);
    std::string new_body_str = insert_top_new_defs_to_body(body_str,
                                                           env.get_all_new_def_var_names(),
                                                           env.get_all_new_defs());

    feature_info.set_extract_method_content(new_body_str);

    config->rewrite_reco_user_info = true;
  }

  recursive_visit (body, &adlog_field_handler, &env);

  if (method_name == "Extract") {
    process_get_bs(&strict_rewriter, &env, find_body_begin_loc(body), body);
    process_deleted_var(&strict_rewriter, &env);

    process_constructor(&strict_rewriter, &env);

    process_update_action(&strict_rewriter, &env);
    process_new_field_def(&strict_rewriter, &env, cxx_method_decl->getEndLoc());

    constructor_info.set_body_content(strict_rewriter.getRewrittenText(constructor_info.body()));

    if (config->use_reco_user_info) {
      std::string reco_body_str = strict_rewriter.getRewrittenText(body);

      std::ostringstream oss_reco;
      oss_reco << "if (bs == nullptr) {\n"
               << "    return;\n"
               << "  }\n\n"
               << env.get_all_new_defs()
               << "\n";

      std::regex p_bs("if (bs == nullptr) \\{[\\s\\S.]*?\\}");
      reco_body_str = std::regex_replace(reco_body_str, p_bs, oss_reco.str());

      feature_info.set_reco_extract_body(reco_body_str);

      std::string normal_body_str = feature_info.extract_method_content();

      std::regex p("(adlog\\.|ad_log\\.)");
      normal_body_str = std::regex_replace(normal_body_str, p, "bslog.");

      size_t pos = normal_body_str.find("{");
      if (pos != std::string::npos) {
        std::ostringstream oss_normal;
        oss_normal << "{\n"
                   << "  if (FLAGS_use_bs_reco_userinfo) {\n"
                   << "    ExtractWithBSRecoUserInfo(bslog, pos, result);\n"
                   << "    return;\n"
                   << "  }\n\n"
                   << "  auto bs = bslog.GetBS();\n"
                   << "    if (bs == nullptr) { return ; }\n    \n"
                   << normal_body_str.substr(pos + 1);

        feature_info.set_extract_method_content(oss_normal.str());
      } else {
        LOG(ERROR) << "cannot find { at begin of body";
      }
    } else {
      feature_info.set_extract_method_content(strict_rewriter.getRewrittenText(body));
    }

    if (!feature_info.is_template()) {
      std::ostringstream oss_ctor;
      oss_ctor << feature_info.feature_name() << "(" << constructor_info.joined_params() << ");";

      strict_rewriter.ReplaceText(constructor_info.source_range(), oss_ctor.str());

      if (config->use_reco_user_info) {
        std::ostringstream oss_extract_reco;
        oss_extract_reco << ";\n"
                         << "void ExtractWithBSRecoUserInfo(const BSLog& bslog, size_t pos,"
                         << " std::vector<ExtractResult>* result);\n";
        strict_rewriter.ReplaceText(body, oss_extract_reco.str());
      } else {
        strict_rewriter.ReplaceText(body, ";");
      }
    }
  } else if (is_infer_filter_method(cxx_method_decl)) {
    // 替换参数 item 为 bslog
    process_infer_filter_param(cxx_method_decl, &strict_rewriter);
    process_infer_filter_root_env(&strict_rewriter, &env, find_body_begin_loc(body));

    auto& infer_filter_funcs = config->infer_filter_funcs;
    infer_filter_funcs[method_name] = rewriter_.getRewrittenText(find_source_range(body));

    strict_rewriter.ReplaceText(find_source_range(body), ";");
  } else {
    std::vector<std::string> params_text;
    process_params(cxx_method_decl, &strict_rewriter, &env, &params_text);
    std::string method_decl = method_name + "(" + absl::StrJoin(params_text, ",\n") + ")";
    bool is_combine_user = feature_info_ptr->is_combine();
    const MethodInfo* method_info = feature_info.find_method_info(method_name);
    if (method_info->is_return_adlog_user_field()) {
      is_combine_user &= true;
    }

    std::string bs_return_type = strict_rewriter.getRewrittenText(cxx_method_decl->getReturnTypeSourceRange());
    if (cxx_method_decl->getReturnType().getTypePtr()->isVoidType() ||
        tool::is_reco_proto(cxx_method_decl->getReturnType())) {
    } else {
      bs_return_type = tool::get_bs_type_str(cxx_method_decl->getReturnType(), is_combine_user);
    }

    feature_info.add_other_method(method_name,
                                  cxx_method_decl->getReturnType(),
                                  bs_return_type,
                                  method_decl,
                                  strict_rewriter.getRewrittenText(body));
    if (!feature_info.is_template()) {
      strict_rewriter.ReplaceText(cxx_method_decl->getSourceRange(),
                                  bs_return_type + " " + method_decl + ";");
    } else {
      strict_rewriter.ReplaceText(cxx_method_decl->getReturnTypeSourceRange(), bs_return_type);
    }
  }

  LOG(INFO) << "process method done, feature_name: " << feature_info_ptr->feature_name()
            << ", method_name: " << method_name;
}

void ExtractMethodVisitor::visit_params(const clang::CXXMethodDecl* cxx_method_decl, Env* env_ptr) {
  if (env_ptr == nullptr) {
    return;
  }

  const std::string& method_name = env_ptr->get_method_name();
  if (method_name == "Extract") {
    return;
  }

  if (auto feature_info = env_ptr->mutable_feature_info()) {
    MethodInfo& method_info = feature_info->touch_method_info(method_name);

    for (size_t i = 0; i < cxx_method_decl->getNumParams(); i++) {
      const clang::ParmVarDecl* param_decl = cxx_method_decl->getParamDecl(i);
      std::string param_name = param_decl->getNameAsString();
      if (param_name.size() == 0) {
        continue;
      }

      if (tool::is_repeated_action_info(param_decl->getType())) {
        method_info.add_new_action_param(i, param_name);
      }
    }
  }
}

void ExtractMethodVisitor::get_overview_info(FeatureInfo* feature_info_ptr,
                                             const clang::CXXMethodDecl* cxx_method_decl,
                                             const std::string& method_name) {
  Env env;
  env.set_method_name(method_name);
  env.set_feature_info(feature_info_ptr);

  visit_params(cxx_method_decl, &env);

  OverviewHandler overview_handler;
  recursive_visit(cxx_method_decl->getBody(), &overview_handler, &env);

  feature_info_ptr->clear_new_field_defs();
}

void ExtractMethodVisitor::visit_loop_init(clang::ForStmt* for_stmt,
                                           Env* env_ptr) {
  clang::Stmt* init_stmt = for_stmt->getInit();
  if (init_stmt == nullptr) {
    LOG(INFO) << "for_stmt has no init!";
    return;
  }

  clang::VarDecl* var_decl = nullptr;
  if (clang::DeclStmt* decl_stmt = dyn_cast<clang::DeclStmt>(init_stmt)) {
    if (decl_stmt->isSingleDecl()) {
      var_decl = dyn_cast<clang::VarDecl>(decl_stmt->getSingleDecl());
    } else {
      var_decl = dyn_cast<clang::VarDecl>(*(decl_stmt->decl_begin()));
    }
  }

  visit_loop_var_decl(var_decl, env_ptr);
}

void ExtractMethodVisitor::visit_loop_init(clang::CXXForRangeStmt* cxx_for_range_stmt,
                                           Env* env_ptr) {
  clang::VarDecl* var_decl = cxx_for_range_stmt->getLoopVariable();
  visit_loop_var_decl(var_decl, env_ptr);
}

void ExtractMethodVisitor::visit_loop_var_decl(clang::VarDecl* var_decl,
                                               Env* env_ptr) {
  if (var_decl != nullptr && var_decl->hasInit()) {
    std::string for_var_name = var_decl->getNameAsString();
    env_ptr->add_loop_var(for_var_name);
    clang::Expr* init_expr = var_decl->getInit();
    env_ptr->add(for_var_name, init_expr);
    LOG(INFO) << "for range, var: " << for_var_name
              << ", expr: " << stmt_to_string(init_expr)
              << ", type: " << var_decl->getType().getAsString()
              << ", loop_var_type; " << tool::get_builtin_type_str(var_decl->getType());
    if (auto& loop_info = env_ptr->cur_mutable_loop_info()) {
      loop_info->set_loop_var_type(tool::get_builtin_type_str(var_decl->getType()));
    }
  } else {
    LOG(INFO) << "loop var_decl is nullptr!";
  }
}

void ExtractMethodVisitor::process_deleted_var(StrictRewriter* rewriter_ptr, Env* env_ptr) {
  if (rewriter_ptr == nullptr || env_ptr == nullptr) {
    return;
  }

  const std::set<std::string>& deleted_vars = env_ptr->deleted_vars();
  for (const std::string& name : deleted_vars) {
    clang::Stmt* decl_stmt = env_ptr->get_decl_stmt(name);
    if (decl_stmt != nullptr) {
      rewriter_ptr->RemoveText(decl_stmt);
    }
  }
}

void ExtractMethodVisitor::process_get_bs(StrictRewriter* rewriter_ptr,
                                          Env* env_ptr,
                                          absl::optional<clang::SourceLocation> body_begin_loc,
                                          clang::Stmt* body) {
  if (rewriter_ptr == nullptr || env_ptr == nullptr) {
    return;
  }

  std::ostringstream oss_get_bs;

  oss_get_bs << "auto bs = bslog.GetBS();\n"
             << "    if (bs == nullptr) { return ; }\n    \n";
  // << env_ptr->get_all_new_defs();
  if (env_ptr->first_if_stmt() != nullptr) {
    if (env_ptr->is_first_if_check_item_pos_include_cond()) {
      LOG(INFO) << "is_first_if_check_item_pos_include_cond: " << env_ptr->is_first_if_check_item_pos_include_cond();
      rewriter_ptr->InsertTextBefore(env_ptr->first_if_stmt()->getBeginLoc(), oss_get_bs.str());
      rewriter_ptr->InsertTextAfterToken(env_ptr->first_if_stmt()->getEndLoc(), env_ptr->get_all_new_defs());
    } else if (env_ptr->is_first_if_check_item_pos_cond()) {
      LOG(INFO) << "is_first_if_check_item_pos_cond: " << env_ptr->is_first_if_check_item_pos_cond();
      rewriter_ptr->ReplaceText(env_ptr->first_if_stmt(), oss_get_bs.str());
      rewriter_ptr->InsertTextAfterToken(env_ptr->first_if_stmt()->getEndLoc(), env_ptr->get_all_new_defs());
    } else {
      // 逻辑有点复杂，先删掉 if 前面的语句，再整体替换。
      std::string text =
        oss_get_bs.str() + env_ptr->get_all_new_defs() + get_rewritten_text_before_first_if(body);
      remove_text_before_first_if(body);
      rewriter_ptr->InsertTextBefore(env_ptr->first_if_stmt()->getBeginLoc(), text);
    }
  } else {
    oss_get_bs << env_ptr->get_all_new_defs()
               << get_rewritten_text_before_first_if(body);

    remove_text_before_first_if(body);

    // StrictRewriter 不能替换两次，因此这个地方不能用 rewriter_ptr
    rewriter_.ReplaceText(find_source_range(body), tool::add_surround_big_parantheses(oss_get_bs.str()));
  }
}

std::string ExtractMethodVisitor::get_rewritten_text_before_first_if(clang::Stmt* body) {
  std::ostringstream oss;

  if (clang::CompoundStmt* compound_stmt = dyn_cast<clang::CompoundStmt>(body)) {
    for (auto it = compound_stmt->body_begin(); it != compound_stmt->body_end(); it++) {
      if (clang::IfStmt* if_stmt = dyn_cast<clang::IfStmt>(*it)) {
        break;
      }

      oss << fix_semicolon(rewriter_.getRewrittenText(find_source_range(*it)));
    }
  }

  return oss.str();
}

void ExtractMethodVisitor::remove_text_before_first_if(clang::Stmt* body) {
  if (clang::CompoundStmt* compound_stmt = dyn_cast<clang::CompoundStmt>(body)) {
    for (auto it = compound_stmt->body_begin(); it != compound_stmt->body_end(); it++) {
      if (clang::IfStmt* if_stmt = dyn_cast<clang::IfStmt>(*it)) {
        return;
      }

      rewriter_.RemoveText(find_source_range(*it));
    }
  }
}

void ExtractMethodVisitor::process_constructor(StrictRewriter* rewriter_ptr, Env* env_ptr) {
  if (rewriter_ptr == nullptr || env_ptr == nullptr) {
    return;
  }

  const auto feature_info = env_ptr->get_feature_info();

  LOG(INFO) << "start process constructor";

  std::ostringstream oss;
  oss << "\n  ";
  if (auto constructor_info = env_ptr->mutable_constructor_info()) {
    constructor_info->fix_common_info_enums();

    std::vector<std::string> to_add;

    for (const std::string& bs_field_enum : constructor_info->bs_field_enums()) {
      to_add.push_back(bs_field_enum);
    }

    for (const auto& common_info : constructor_info->common_info_enums()) {
      std::string bs_enum_str = common_info.bs_enum_str();
      if (bs_enum_str.size() > 0) {
        if (common_info.if_stmt_end()) {
          std::ostringstream oss_enum;
          oss_enum << "attr_metas_.emplace_back(BSFieldEnum::"
                   << bs_enum_str
                   << ");\n    ";
          rewriter_ptr->InsertTextBefore(common_info.if_stmt_end().value(), oss_enum.str());
        } else if (bs_enum_str.size() > 0) {
          to_add.push_back(bs_enum_str);
        }
      }

      if (feature_info != nullptr) {
        if (feature_info->has_common_info_multi_int_list()) {
          if (absl::optional<int> enum_value = find_common_attr_int_value(common_info.enum_ref())) {
            rewriter_ptr->ReplaceText(common_info.enum_ref(), std::to_string(*enum_value));
          } else {
            LOG(INFO) << "cannot find common info enum from expr: "
                      << stmt_to_string(common_info.enum_ref());
          }
        }
      }
    }

    auto compare_str = [](const std::string& a, const std::string& b) {
      if (a.size() != b.size()) {
        return a.size() < b.size();
      }
      return a < b;
    };

    int reco_field_cnt = 0;
    std::sort(to_add.begin(), to_add.end(), compare_str);
    for (size_t i = 0; i < to_add.size(); i++) {
      if (!tool::is_str_from_reco_user_info(to_add[i])) {
        oss << "attr_metas_.emplace_back(BSFieldEnum::" << to_add[i] << ");\n    ";
      } else {
        reco_field_cnt += 1;
      }
    }

    if (reco_field_cnt > 0) {
      for (size_t i = 0; i < to_add.size(); i++) {
        if (tool::is_str_from_reco_user_info(to_add[i])) {
          oss << "attr_metas_.emplace_back(BSFieldEnum::" << to_add[i]
              << ");\n    ";
        }
      }
    }

    oss << "\n";

    for (const std::string& leaf : constructor_info->middle_node_leafs()) {
      oss << leaf << ".fill_attr_metas(&attr_metas_);\n    ";
    }

    if (constructor_info->has_get_norm_query()) {
      oss << "bs_util.BSFillNormQueryAttrMeta(&attr_metas_);\n    ";
    }

    if (feature_info != nullptr) {
      const auto& new_field_defs = feature_info->new_field_defs();
      for (auto it = new_field_defs.begin(); it != new_field_defs.end(); it++) {
        if (tool::is_from_info_util(it->second.name())) {
          oss << it->second.name() << ".fill_attr_metas(&attr_metas_);\n    ";
        }
        if (tool::is_from_info_util(it->second.exists_name())) {
          oss << it->second.exists_name() << ".fill_attr_metas(&attr_metas_);\n    ";
        }
      }
    }

    rewriter_ptr->InsertTextBefore(constructor_info->body_end(), oss.str());
  }
}

void ExtractMethodVisitor::process_update_action(StrictRewriter* rewriter_ptr, Env* env_ptr) {
  if (rewriter_ptr == nullptr || env_ptr == nullptr) {
    return;
  }

  if (const auto feature_info = env_ptr->get_feature_info()) {
    clang::BinaryOperator* update_action_stmt = feature_info->find_update_action_stmt();
    if (update_action_stmt != nullptr) {
      std::vector<std::string> arr = absl::StrSplit(stmt_to_string(update_action_stmt), "=");
      if (arr.size() == 2) {
        std::string x = std::regex_replace(arr[1], std::regex(" "), "");
        if (is_integer(x)) {
          int action = std::stoi(x);
          LOG(INFO) << "find action: " << action
                    << ", update_action_stmt: " << stmt_to_string(update_action_stmt);

          std::ostringstream oss;
          const auto& new_field_defs = feature_info->new_field_defs();

          for (auto it = new_field_defs.begin(); it != new_field_defs.end(); it++) {
            if (const auto& expr_type = it->second.expr_type()) {
              if (*expr_type == ExprType::ACTION_DETAIL_FIXED_GET) {
                oss << it->second.name() << ".Update(" << action << ");\n    ";
              } else if (*expr_type == ExprType::ACTION_DETAIL_FIXED_HAS) {
                oss << it->second.name() << ".UpdateHas(" << action << ");\n    ";
              }
            }
          }

          if (oss.str().size() > 0) {
            rewriter_ptr->ReplaceText(update_action_stmt, oss.str());
          }
        }
      }
    }
  }
}

void ExtractMethodVisitor::process_new_field_def(StrictRewriter* rewriter_ptr,
                                                 Env* env_ptr,
                                                 clang::SourceLocation extract_method_end) {
  if(rewriter_ptr == nullptr || env_ptr == nullptr) {
    return;
  }


  if (const auto feature_info = env_ptr->get_feature_info()) {
    const std::unordered_map<std::string, NewVarDef>& new_field_defs = feature_info->new_field_defs();
    if (new_field_defs.size() == 0) {
      return;
    }

    std::ostringstream oss;
    oss << "\n\n private:\n";

    for (auto it = new_field_defs.begin(); it != new_field_defs.end(); it++) {
      if (it->second.var_def().size() > 0) {
        oss << it->second.var_def() << ";\n";
      }
      if (it->second.exists_var_def().size() > 0) {
        oss << it->second.exists_var_def() << ";\n";
      }
    }

    if (oss.str().size() > 0) {
      // 注意: 有些模板类参数来自 field, 因此新增 field_def 必须放到 field_end_loc 之后。
      std::string new_text = oss.str();

      if (absl::optional<clang::SourceLocation> field_end_loc = feature_info->last_field_decl_end_log()) {
        rewriter_ptr->InsertTextAfterToken(*field_end_loc, std::string(";") + new_text);
      } else {
        if (feature_info->has_cc_file()) {
          new_text = std::string(";") + new_text;
        }
        rewriter_ptr->InsertTextAfterToken(extract_method_end, new_text);
      }
    }
  }
}

absl::optional<clang::SourceLocation> ExtractMethodVisitor::find_body_begin_loc(clang::Stmt* stmt) {
  if (stmt == nullptr) {
    return absl::nullopt;
  }

  if (clang::CompoundStmt* compound_stmt = dyn_cast<clang::CompoundStmt>(stmt)) {
    for (clang::CompoundStmt::body_iterator start = compound_stmt->body_begin();
         start != compound_stmt->body_end(); start++) {
      return absl::optional<clang::SourceLocation>((*start)->getBeginLoc());
    }
  }

  return absl::optional<clang::SourceLocation>(stmt->getBeginLoc());
}

void ExtractMethodVisitor::reset_rewrite_buffer(Env* env_ptr) {
  if (const auto feature_info = env_ptr->get_feature_info()) {
    clang::FileID file_id = feature_info->file_id();
    clang::RewriteBuffer& rewrite_buffer = rewriter_.getEditBuffer(file_id);
    const std::string& origin_buffer = feature_info->origin_buffer();
    rewrite_buffer.Initialize(origin_buffer.data(), origin_buffer.data() + origin_buffer.size());
  }
}

void ExtractMethodVisitor::process_params(const clang::CXXMethodDecl* cxx_method_decl,
                                          StrictRewriter* rewriter_ptr,
                                          Env* env_ptr,
                                          std::vector<std::string>* params_text_ptr) {
  if (cxx_method_decl == nullptr ||
      rewriter_ptr == nullptr ||
      env_ptr == nullptr ||
      params_text_ptr == nullptr) {
    return;
  }

  const std::string& method_name = cxx_method_decl->getNameAsString();
  if (method_name == "Extract") {
    return;
  }

  if (auto feature_info = env_ptr->mutable_feature_info()) {
    MethodInfo& method_info = feature_info->touch_method_info(method_name);
    for (size_t i = 0; i < cxx_method_decl->getNumParams(); i++) {
      const clang::ParmVarDecl* param_decl = cxx_method_decl->getParamDecl(i);
      std::string param_name = param_decl->getNameAsString();
      if (param_name.size() == 0) {
        continue;
      }

      if (param_name == "adlog" || param_name == "ad_log") {
        params_text_ptr->push_back("const BSLog& bslog");
      } else {
        if (tool::is_repeated_action_info(param_decl->getType())) {
          const NewActionParam& new_action_param = method_info.find_new_action_param(i);
          if (new_action_param.origin_name() == param_name) {
            const std::vector<NewActionFieldParam>& new_params = new_action_param.new_params();
            for (size_t j = 0; j < new_params.size(); j++) {
              std::ostringstream oss;

              if (new_params[j].field() == "size") {
                oss << new_params[j].inner_type_str() << " " << new_params[j].name();
              } else {
                oss << new_params[j].const_ref_str();
              }

              params_text_ptr->push_back(oss.str());
            }
          }
        } else if (tool::is_common_info_struct(param_decl->getType())) {
          // 目前都是 user 特征，不需要区分 combine
          // 如: helper 函数
          // void helper(FeaturePrefix prefix,
          //             const ::bs::auto_cpp_rewriter::CommonInfoAttr& userAttr,
          //             std::vector<ExtractResult>* result);
          if (const auto& common_info_prepare = method_info.common_info_prepare()) {
            if (const auto& common_info_method_name = common_info_prepare->method_name()) {
              std::ostringstream oss;

              std::string bs_type_str = CommonAttrInfo::get_bs_type_str(*common_info_method_name, false);
              oss << "const " << bs_type_str << "& " << param_name;

              params_text_ptr->push_back(oss.str());

              if (feature_info->is_template()) {
                rewriter_ptr->ReplaceText(param_decl->getSourceRange(), oss.str());
              }
            } else {
              LOG(INFO) << "cannot find common_inof_method_name from common info prepare, method_name: "
                        << method_name;
            }
          } else {
            LOG(INFO) << "cannot find common info prepare, method_name: " << method_name;
          }
        } else {
          LOG(INFO) << "param, i: " << i << ", type: " << param_decl->getType().getAsString()
                    << ", param_name: " << param_name;
          params_text_ptr->push_back(param_decl->getType().getAsString() + " " + param_name);
        }
      }
    }
  }
}

bool ExtractMethodVisitor::is_infer_filter_method(const clang::CXXMethodDecl* cxx_method_decl) {
  if (cxx_method_decl == nullptr || cxx_method_decl->getNumParams() != 2) {
    return false;
  }

  const clang::ParmVarDecl* param0 = cxx_method_decl->getParamDecl(0);
  const clang::ParmVarDecl* param1 = cxx_method_decl->getParamDecl(1);

  // 更准确的判断需要加上类型，先简单处理，只根据参数名称判断。
  if (param0->getNameAsString() == "item" && param1->getNameAsString() == "filter_condition") {
    return true;
  }

  return false;
}

void ExtractMethodVisitor::process_infer_filter_param(const clang::CXXMethodDecl* cxx_method_decl,
                                                      StrictRewriter* rewriter_ptr) {
  if (cxx_method_decl == nullptr || rewriter_ptr == nullptr) {
    return;
  }

  if (cxx_method_decl->getNumParams() == 2) {
    const clang::ParmVarDecl* param0 = cxx_method_decl->getParamDecl(0);
    const clang::ParmVarDecl* param1 = cxx_method_decl->getParamDecl(1);

    rewriter_ptr->ReplaceText(param0->getSourceRange(), "const SampleInterface& bs");
    rewriter_ptr->ReplaceText(param1->getSourceRange(), "const FilterCondition& filter_condition, size_t pos");
  }
}

void ExtractMethodVisitor::process_infer_filter_root_env(
  StrictRewriter* rewriter_ptr,
  Env* env_ptr,
  absl::optional<clang::SourceLocation> body_begin_loc) {
  if (rewriter_ptr == nullptr || env_ptr == nullptr) {
    return;
  }

  if (!body_begin_loc) {
    return;
  }

  rewriter_ptr->InsertTextBefore(*body_begin_loc, env_ptr->get_all_new_defs());
}

std::string ExtractMethodVisitor::insert_top_new_defs_to_body(
  const std::string& body_str,
  const std::vector<std::string>& new_def_var_names,
  const std::string& new_defs
) const {
  size_t pos = std::string::npos;

  for (size_t i = 0; i < new_def_var_names.size(); i++) {
    size_t index = body_str.find(new_def_var_names[i]);
    if (index < pos) {
      pos = index;
    }
  }

  if (pos != std::string::npos && pos < body_str.size()) {
    int i = pos;
    for (; i > 0; i--) {
      // 最早出现的前一行
      if (body_str[i] == '\n') {
        break;
      }
    }

    if (i > 0 && i < body_str.size()) {
      std::ostringstream oss;
      oss << body_str.substr(0, i)
          << "\n"
          << new_defs
          << "\n"
          << body_str.substr(i);

      return oss.str();
    }
  }

  LOG(ERROR) << "cannot find new def var names in body_str, new_def_var_names: "
             << absl::StrJoin(new_def_var_names, ", ");
  return body_str;
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
