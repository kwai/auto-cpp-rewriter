#include "VarDeclInfo.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

void VarDeclInfo::add(const std::string& name, clang::Expr* init_expr) {
  var_decls_.emplace(name, DeclInfo(name, init_expr));
}

void VarDeclInfo::add(const std::string& name, clang::Expr* init_expr, clang::DeclStmt* decl_stmt) {
  var_decls_.emplace(name, DeclInfo(name, init_expr, decl_stmt));
}

clang::Expr* VarDeclInfo::find(const std::string& name) {
  auto it = var_decls_.find(name);
  if (it == var_decls_.end()) {
    return nullptr;
  }

  return it->second.init_expr();
}

void VarDeclInfo::update(const std::map<std::string, clang::Expr*>& decls) {
  for (auto it = decls.begin(); it != decls.end(); it++) {
    var_decls_.emplace(it->first, DeclInfo(it->first, it->second));
  }
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
