#include <nlohmann/json.hpp>
#include <ostream>
#include <regex>
#include <glog/logging.h>
#include <absl/strings/str_join.h>
#include <absl/strings/str_split.h>
#include <sstream>
#include "../Env.h"
#include "../Tool.h"
#include "../handler/LogicHandler.h"
#include "../handler/BSFieldHandler.h"
#include "./BSExtractMethodVisitor.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

using nlohmann::json;
using tool::replace_simple_text;
using tool::insert_str_after_bs_equal_end;

json BSExtractMethodVisitor::visit(const clang::CXXMethodDecl* cxx_method_decl,
                                    FeatureInfo* feature_info_ptr) {
  auto config = GlobalConfig::Instance();

  std::string method_name = cxx_method_decl->getNameAsString();

  if (config->dump_ast) {
    LOG(INFO) << "dump method: " << method_name;
    cxx_method_decl->dump();
  }

  if (starts_with(method_name, "~") ||
      starts_with(method_name, "operator") ||
      method_name == feature_info_ptr->feature_name()) {
    return nullptr;
  }

  Env env;
  env.set_feature_info(feature_info_ptr);
  env.add_template_var_names(feature_info_ptr->template_var_names());

  LOG(INFO) << "========================= start process method, name: " << method_name
            << " ==================================";

  clang::Stmt *body = cxx_method_decl->getBody();

  BSFieldHandler bs_field_handler(rewriter_);
  recursive_visit(body, &bs_field_handler, &env);

  LogicHandler logic_handler;
  auto root = recursive_visit (body, &logic_handler, &env);

  insert_new_def_at_begin(body, &env);

  if (root != nullptr) {
  } else {
    LOG(INFO) << "logic is nullptr!";
  }

  LOG(INFO) << "process method done, feature_name: " << feature_info_ptr->feature_name()
            << ", method_name: " << method_name;

  return root;
}

void BSExtractMethodVisitor::visit_params(const clang::CXXMethodDecl* cxx_method_decl, Env* env_ptr) {
}

json BSExtractMethodVisitor::visit_loop_init(clang::ForStmt* for_stmt,
                                             Env* env_ptr) {
  auto root = json::array();
  clang::Stmt* init_stmt = for_stmt->getInit();
  if (init_stmt == nullptr) {
    LOG(INFO) << "for_stmt has no init!";
    return nullptr;
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

  return root;
}

json BSExtractMethodVisitor::visit_loop_init(clang::CXXForRangeStmt* cxx_for_range_stmt,
                                             Env* env_ptr) {
  auto root = json::array();
  clang::VarDecl* var_decl = cxx_for_range_stmt->getLoopVariable();

  visit_loop_var_decl(var_decl, env_ptr);

  return root;
}

json BSExtractMethodVisitor::visit_loop_var_decl(clang::VarDecl* var_decl,
                                                 Env* env_ptr) {
  if (var_decl != nullptr && var_decl->hasInit()) {
    std::string for_var_name = var_decl->getNameAsString();

    auto root = json::array();

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

    return root;
  } else {
    LOG(INFO) << "loop var_decl is nullptr!";

    return nullptr;
  }
}

void BSExtractMethodVisitor::insert_new_def_at_begin(clang::Stmt* body, Env* env_ptr) {
  if (body == nullptr || env_ptr == nullptr) {
    return;
  }

  std::string body_str = rewriter_.getRewrittenText(body->getSourceRange());

  std::string new_defs_str = env_ptr->get_all_new_defs();
  if (new_defs_str.size() > 0) {
    std::string s = insert_str_after_bs_equal_end(body_str, new_defs_str);
    rewriter_.ReplaceText(body->getSourceRange(), replace_simple_text(s));
  }
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
