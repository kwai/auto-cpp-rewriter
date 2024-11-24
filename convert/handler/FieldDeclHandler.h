#pragma once

#include <vector>
#include <memory>

#include "clang/AST/AST.h"
#include "clang/AST/Expr.h"
#include "clang/AST/Stmt.h"
#include "clang/Basic/SourceLocation.h"

#include "StrictRewriter.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class Env;

/// 处理 FieldDecl
class FieldDeclHandler {
 public:
  explicit FieldDeclHandler(clang::Rewriter& rewriter): rewriter_(rewriter) {}   // NOLINT

  template<typename T>
  void process(T t, Env* env_ptr) {}

  void process(clang::InitListExpr *init_list_expr, Env* env_ptr);
  void process(clang::DeclRefExpr* decl_ref_expr, Env* env_ptr);

 private:
  StrictRewriter rewriter_;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
