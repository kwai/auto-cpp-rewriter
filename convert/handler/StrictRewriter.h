#pragma once

#include <glog/logging.h>

#include <type_traits>
#include <string>
#include <utility>
#include <vector>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include "clang/AST/Stmt.h"
#include "clang/AST/Expr.h"
#include "clang/Rewrite/Core/Rewriter.h"

#include "../Tool.h"
#include "LazyReplace.h"
#include "RewriteAction.h"

class ExprInfo;

namespace ks {
namespace ad_algorithm {
namespace convert {

/// 不能被替换两次，需要严格记录被替换的表达式。
class StrictRewriter {
 public:
  explicit StrictRewriter(clang::Rewriter &rewriter): rewriter_(rewriter) {}      // NOLINT
  explicit StrictRewriter(clang::Rewriter &rewriter, const std::string &rule_name)    // NOLINT
    : rewriter_(rewriter), rule_name_(rule_name) {}

  /// 为了和 clang::Rewriter 接口保持一致，用驼峰命名
  bool ReplaceText(clang::Stmt *stmt, const std::string &s);
  bool ReplaceText(clang::SourceRange range, const std::string &s);
  bool InsertTextBefore(clang::Stmt *stmt, const std::string &s);
  bool InsertTextBefore(clang::SourceLocation loc, const std::string &s);
  bool InsertTextAfter(clang::SourceLocation loc, const std::string &s);
  bool InsertTextAfterToken(clang::SourceLocation loc, const std::string &s);
  bool RemoveText(clang::Stmt *stmt);
  bool RemoveText(clang::SourceRange range);
  std::string getRewrittenText(clang::Stmt *stmt) const;
  std::string getRewrittenText(clang::Expr *expr) const;
  std::string getRewrittenText(clang::SourceRange range) const;
  std::string getRewrittenText(ExprInfo* expr_info_ptr) const;

  /// 例: thread_local static std::vector<::auto_cpp_rewriter::CommonInfoAttr>
  /// action_list(user_attr_map_size); 有些声明的替换需要在 common info enum
  /// 确定后才能知道具体的类型是 int 还是 float, 因此先将替换逻辑保存起来。 等
  /// common info enum 确定后再执行。
  template <typename... Args> void emplace_lazy_replace(Args... args) {
    lazy_replaces_.emplace_back(std::forward<Args>(args)...);
  }

  void run_lazy_replace();
  bool is_replace_visited(clang::Stmt* stmt);

 private:
  template<typename T>
  bool is_visited(const T& v, clang::Stmt* stmt) {
    return v.find(pointer_to_str(stmt)) != v.end();
  }

 private:
  clang::Rewriter& rewriter_;
  std::string rule_name_;
  std::unordered_set<std::string> visited_;
  std::unordered_set<std::string> visited_delete_;
  std::unordered_set<std::string> visited_insert_before_;
  std::vector<LazyReplace> lazy_replaces_;
  std::vector<RewriteAction> rewrite_actions_;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
