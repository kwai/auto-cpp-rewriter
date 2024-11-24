#pragma once

#include <absl/types/optional.h>

#include <map>
#include <string>
#include <unordered_map>

#include "clang/AST/Expr.h"
#include "clang/AST/Stmt.h"

#include "../Type.h"
#include "DeclInfo.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class VarDeclInfo {
 public:
  VarDeclInfo() = default;

  void add(const std::string& name, clang::Expr* init_expr);
  void add(const std::string& name, clang::Expr* init_expr, clang::DeclStmt* decl_stmt);
  clang::Expr* find(const std::string& name);

  const std::unordered_map<std::string, DeclInfo>& var_decls() const { return var_decls_; }

  void update(const std::map<std::string, clang::Expr*>& decls);

 private:
  std::unordered_map<std::string, DeclInfo> var_decls_;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
