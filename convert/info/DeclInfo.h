#pragma once

#include <absl/types/optional.h>
#include <map>
#include <unordered_map>
#include <string>
#include "clang/AST/Expr.h"
#include "../Type.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class Env;

/// 保存当前 decl_stmt 相关的信息。
class DeclInfo {
 public:
  DeclInfo() = default;
  explicit DeclInfo(const std::string& name): name_(name), init_expr_(nullptr) {}
  explicit DeclInfo(const std::string& name, clang::Expr* init_expr): name_(name), init_expr_(init_expr) {}
  explicit DeclInfo(const std::string& name, clang::Expr* init_expr, clang::DeclStmt* decl_stmt):
    name_(name), init_expr_(init_expr), decl_stmt_(decl_stmt) {}

  const std::string& name() const { return name_; }

  clang::Expr* init_expr() const { return init_expr_; }
  void set_init_expr(clang::Expr* init_expr) { init_expr_ = init_expr; }

  clang::DeclStmt* decl_stmt() const { return decl_stmt_; }

 private:
  std::string name_;
  clang::Expr* init_expr_ = nullptr;
  clang::DeclStmt* decl_stmt_ = nullptr;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
