#pragma once

#include "clang/AST/DeclCXX.h"
#include "clang/Rewrite/Core/Rewriter.h"

#include "../info/FeatureInfo.h"
#include "../info/ConstructorInfo.h"
#include "../handler/StrictRewriter.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class Env;

class CtorVisitor {
 public:
  explicit CtorVisitor(clang::Rewriter &rewriter) :         // NOLINT
    rewriter_(rewriter) {
  }

  void visit(clang::CXXConstructorDecl* cxx_constructor_decl, FeatureInfo* feature_info_ptr);
  void recursive_visit(clang::Stmt* stmt, FeatureInfo* info_ptr, Env* env_ptr);

 private:
  void process(clang::CXXMemberCallExpr* cxx_member_call_expr, FeatureInfo* feature_info_ptr, Env* env_ptr);

 protected:
  StrictRewriter rewriter_;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks