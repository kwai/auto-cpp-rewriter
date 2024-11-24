#pragma once

#include "clang/AST/Decl.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "../handler/StrictRewriter.h"
#include "../handler/FieldDeclHandler.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class Env;
class FeatureInfo;

class FieldDeclVisitor {
 public:
  explicit FieldDeclVisitor(clang::Rewriter &rewriter) :    // NOLINT
    rewriter_(rewriter) {
  }

  void visit(const clang::FieldDecl* field_decl, FeatureInfo* feature_info_ptr);
  void recursive_visit(clang::Stmt *stmt, FieldDeclHandler* handler_ptr, Env *env_ptr);

 protected:
  clang::Rewriter& rewriter_;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
