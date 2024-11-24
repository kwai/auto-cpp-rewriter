#pragma once

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/Tooling.h"
// Declares llvm::cl::extrahelp.
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/AST/Expr.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class FeatureInfo;

/// 解析 BS 类的逻辑。
class BSFeatureDeclCallback : public clang::ast_matchers::MatchFinder::MatchCallback {
 public:
  BSFeatureDeclCallback() = default;
  explicit BSFeatureDeclCallback(clang::Rewriter &rewriter): rewriter_(rewriter) {}     // NOLINT

  void run(const clang::ast_matchers::MatchFinder::MatchResult &Result);

  void process_ctor(clang::CXXConstructorDecl* cxx_constructor_decl, FeatureInfo* feature_info_ptr);
  void process_field(clang::FieldDecl* field_decl, FeatureInfo* feature_info_ptr);
  void process_method(clang::CXXMethodDecl* cxx_method_decl, FeatureInfo* feature_info_ptr);
  void process_all_methods(const clang::CXXRecordDecl* cxx_record_decl, FeatureInfo* feature_info_ptr);

 private:
  clang::Rewriter& rewriter_;
  const std::string EXTRACT = "Extract";
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
