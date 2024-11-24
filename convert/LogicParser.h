#pragma once

#include <nlohmann/json.hpp>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/Tooling.h"

#include "matcher_callback/BSTypeAliasCallback.h"
#include "matcher_callback/BSFeatureDeclCallback.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

/// 解析 BS 特征逻辑。
class LogicConsumer : public clang::ASTConsumer {
 public:
  explicit LogicConsumer(clang::Rewriter &R);              // NOLINT
  void HandleTranslationUnit(clang::ASTContext &Context) override;  // NOLINT

 public:
  clang::ast_matchers::MatchFinder bs_match_finder_;
  clang::ast_matchers::MatchFinder bs_type_alias_finder_;

  BSFeatureDeclCallback bs_feature_decl_callback_;
  BSTypeAliasCallback bs_type_alias_callback_;
};

class LogicParser : public clang::ASTFrontendAction {
 public:
  void EndSourceFileAction() override;

  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &CI,       // NOLINT
                                                        llvm::StringRef file) override;
 private:
  clang::Rewriter rewriter_;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
