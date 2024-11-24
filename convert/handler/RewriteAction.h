#pragma once

#include <glog/logging.h>

#include <type_traits>
#include <string>
#include <vector>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include "clang/AST/Stmt.h"
#include "clang/AST/Expr.h"
#include "clang/Rewrite/Core/Rewriter.h"

#include "../Tool.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class RewriteAction {
 public:
  explicit RewriteAction(clang::Stmt* stmt, const std::string& replace_text, const std::string& origin_text):
    stmt_(stmt), replace_text_(replace_text), origin_text_(origin_text) {}

  clang::Stmt* stmt() const { return stmt_; }
  const std::string& replace_text() const { return replace_text_; }
  const std::string& origin_text() const { return origin_text_; }

 private:
  clang::Stmt* stmt_;
  std::string replace_text_;
  std::string origin_text_;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
