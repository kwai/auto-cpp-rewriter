#include <absl/strings/str_join.h>
#include <absl/strings/str_split.h>
#include <gflags/gflags.h>
#include <glog/logging.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <regex>

#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/Tooling.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/AST/AST.h"
#include "clang/Lex/Lexer.h"
#include "clang/AST/ASTConsumer.h"

#include "LogicParser.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

using clang::TK_IgnoreUnlessSpelledInSource;
using clang::ast_matchers::decl;
using clang::ast_matchers::namedDecl;
using clang::ast_matchers::matchesName;
using clang::ast_matchers::typeAliasDecl;
using clang::ast_matchers::traverse;
using clang::ast_matchers::cxxRecordDecl;
using clang::ast_matchers::isDerivedFrom;
using clang::ast_matchers::unless;
using clang::ast_matchers::hasName;
using clang::ast_matchers::isExpandedFromMacro;
using clang::ast_matchers::isDerivedFrom;

LogicConsumer::LogicConsumer(clang::Rewriter &R):
  bs_feature_decl_callback_(R) {
  // 目前只能匹配到 typeAliasDecl(), 可能会有更好的匹配。
  auto BSTypeAliasMatcher = decl(typeAliasDecl(),
                                 namedDecl(matchesName("BSExtract.*"))).bind("BSTypeAlias");
  bs_type_alias_finder_.addMatcher(BSTypeAliasMatcher, &bs_type_alias_callback_);

  // TK_IgnoreUnlessSpelledInSource 用来忽略模板
  auto BSFeatureDeclMatcher = traverse(TK_IgnoreUnlessSpelledInSource,
                                       cxxRecordDecl(isDerivedFrom(hasName("BSFastFeature")),
                                                     unless(isExpandedFromMacro("DISALLOW_COPY_AND_ASSIGN")),
                                                     unless(isExpandedFromMacro("REGISTER_BS_EXTRACTOR")))).bind("BSFeatureDecl");  // NOLINT

  bs_match_finder_.addMatcher(BSFeatureDeclMatcher, &bs_feature_decl_callback_);
}

void LogicConsumer::HandleTranslationUnit(clang::ASTContext &Context) {
  // 解析模板参数, 这一步必须在解析 Extract 之前, 因为这些参数会被用到。
  bs_type_alias_finder_.matchAST(Context);

  // 解析 BSExtract 逻辑。
  bs_match_finder_.matchAST(Context);
}

void LogicParser::EndSourceFileAction() {
  auto config = GlobalConfig::Instance();
  {
    std::lock_guard<std::mutex> lock(config->mu);

    std::string cmd_format("clang-format "
                           "--style=\"{BasedOnStyle: Google, ColumnLimit: 110, IndentCaseLabels: true}\" -i ");  // NOLINT

    for (auto it = config->feature_info.begin(); it != config->feature_info.end(); it++) {
      const std::string& bs_extractor_name = it->first;
      const FeatureInfo& feature_info = it->second;

      const std::string& origin_file = feature_info.origin_file();
      if (origin_file.size() == 0) {
        LOG(INFO) << "origin_file is empty! feature_name: " << bs_extractor_name;
        continue;
      }

      if (rewriter_.overwriteChangedFiles()) {
        LOG(INFO) << "rewrite to file: " << origin_file;
      }

      std::system((cmd_format + origin_file).c_str());
      if (const auto& cc_filename = it->second.cc_filename()) {
        std::system((cmd_format + cc_filename.value()).c_str());
      }
    }
  }
}

std::unique_ptr<clang::ASTConsumer> LogicParser::CreateASTConsumer(clang::CompilerInstance &CI,   // NOLINT
                                                                   llvm::StringRef file) {
  rewriter_.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());

  return std::make_unique<LogicConsumer>(rewriter_);
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
