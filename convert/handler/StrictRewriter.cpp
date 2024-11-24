#include "../ExprInfo.h"
#include "../Tool.h"
#include "StrictRewriter.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

bool StrictRewriter::ReplaceText(clang::Stmt* stmt, const std::string& s) {
  if (is_visited(visited_, stmt)) {
    return false;
  }

  visited_.insert(pointer_to_str(stmt));
  return rewriter_.ReplaceText(find_source_range(stmt), s);
}

bool StrictRewriter::ReplaceText(clang::SourceRange range, const std::string& s) {
  return rewriter_.ReplaceText(range, s);
}

bool StrictRewriter::InsertTextBefore(clang::Stmt* stmt, const std::string& s) {
  if (is_visited(visited_insert_before_, stmt)) {
    return false;
  }

  visited_insert_before_.insert(pointer_to_str(stmt));
  std::string origin_text = rewriter_.getRewrittenText(find_source_range(stmt));
  return rewriter_.ReplaceText(find_source_range(stmt), s + origin_text);
}

bool StrictRewriter::InsertTextBefore(clang::SourceLocation loc, const std::string& s) {
  return rewriter_.InsertTextBefore(loc, s);
}

bool StrictRewriter::InsertTextAfter(clang::SourceLocation loc, const std::string& s) {
  return rewriter_.InsertTextAfter(loc, s);
}

bool StrictRewriter::InsertTextAfterToken(clang::SourceLocation loc, const std::string& s) {
  return rewriter_.InsertTextAfterToken(loc, s);
}

bool StrictRewriter::RemoveText(clang::Stmt* stmt) {
  if (is_visited(visited_delete_, stmt)) {
    return false;
  }

  visited_delete_.insert(pointer_to_str(stmt));
  return rewriter_.RemoveText(find_source_range(stmt));
}

bool StrictRewriter::RemoveText(clang::SourceRange range) {
  return rewriter_.RemoveText(range);
}

std::string StrictRewriter::getRewrittenText(clang::Stmt* stmt) const {
  return rewriter_.getRewrittenText(find_source_range(stmt));
}

std::string StrictRewriter::getRewrittenText(clang::Expr* expr) const {
  return rewriter_.getRewrittenText(find_source_range(expr));
}

std::string StrictRewriter::getRewrittenText(clang::SourceRange range) const {
  return rewriter_.getRewrittenText(range);
}

std::string StrictRewriter::getRewrittenText(ExprInfo* expr_info_ptr) const {
  if (expr_info_ptr == nullptr) {
    return "";
  }

  if (expr_info_ptr->is_decl_ref_expr()) {
    return getRewrittenText(expr_info_ptr->origin_expr());
  } else {
    return getRewrittenText(expr_info_ptr->expr());
  }
}

void StrictRewriter::run_lazy_replace() {
  for (size_t i = 0; i < lazy_replaces_.size(); i++) {
    StmtReplacement res = lazy_replaces_[i].run();
    ReplaceText(res.first, res.second);
  }
}

bool StrictRewriter::is_replace_visited(clang::Stmt* stmt) {
  return is_visited(visited_, stmt);
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
