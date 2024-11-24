#include <string>

#include "../Tool.h"
#include "RuleBase.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

std::string RuleBase::get_loop_body(clang::Stmt* body, bool skip_if) {
  if (body == nullptr) {
    return "";
  }

  std::ostringstream oss_text;
  for (auto it_body = body->child_begin(); it_body != body->child_end(); it_body++) {
    if (skip_if) {
      if (clang::IfStmt* if_stmt = dyn_cast<clang::IfStmt>(*it_body)) {
        continue;
      }
    }
    std::string new_text = rewriter_.getRewrittenText(*it_body);
    oss_text << fix_semicolon(new_text) << "\n";
  }

  return oss_text.str();
}

std::string RuleBase::get_loop_body_without_if(clang::Stmt* body) {
  return get_loop_body(body, true);
}

std::string RuleBase::get_complete_rewritten_text(clang::Stmt* body, Env* env_ptr) {
  std::ostringstream oss;

  process_deleted_var(env_ptr);

  oss << env_ptr->get_all_new_defs() << "\n";
  oss << rewriter_.getRewrittenText(find_source_range(body));

  return oss.str();
}

std::string RuleBase::get_compound_stmt_rewritten_text(clang::Stmt* stmt) {
  if (stmt == nullptr) {
    return "";
  }

  std::ostringstream oss;
  if (clang::CompoundStmt* compound_stmt = dyn_cast<clang::CompoundStmt>(stmt)) {
    oss << rewriter_.getRewrittenText(compound_stmt);
  }

  return tool::rm_surround_big_parantheses(oss.str());
}

void RuleBase::process_deleted_var(Env* env_ptr) {
  // delete vars
  const std::set<std::string>& deleted_vars = env_ptr->deleted_vars();
  for (const std::string& name : deleted_vars) {
    if (starts_with(name, "__") || starts_with(name, "* __")) {
      continue;
    }

    clang::Stmt* decl_stmt = env_ptr->get_decl_stmt(name);
    if (decl_stmt != nullptr) {
      LOG(INFO) << "delete var, name: " << name
                << ", stmt: " << stmt_to_string(decl_stmt);
      rewriter_.RemoveText(decl_stmt);
    }
  }
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
