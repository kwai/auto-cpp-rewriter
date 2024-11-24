#pragma once

#include <nlohmann/json.hpp>

#include <map>
#include <set>
#include <memory>
#include <string>

#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Lex/Lexer.h"

#include "clang/Lex/Preprocessor.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/Frontend/CompilerInstance.h"
#include "llvm/ADT/StringRef.h"

#include "Tool.h"
#include "info/FeatureInfo.h"
#include "matcher_callback/FeatureDeclCallback.h"
#include "matcher_callback/TypeAliasCallback.h"
#include "matcher_callback/InferFilterCallback.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

/// 删除代码中的注释。
class RmComment: public clang::CommentHandler {
 public:
  explicit RmComment(clang::Rewriter& rewriter): rewriter(rewriter) {}

  bool HandleComment(clang::Preprocessor& preprocessor,
                     clang::SourceRange source_range) override;

 private:
  clang::Rewriter &rewriter;
};

/// 处理 AST 节点。
class ConvertASTConsumer : public clang::ASTConsumer {
 public:
  ConvertASTConsumer(clang::Rewriter &R);
  void HandleTranslationUnit(clang::ASTContext &Context) override;

 private:
  clang::Rewriter& rewriter_;

  /// 用于匹配 `FeatureDecl` 的 `MatchFinder`。
  ///
  /// 所有的特征都继承自同一个基类 `FastFeature`，我们可以使用同一个 `MatchFinder` 来匹配所有的特征。
  ///
  /// 示例:
  /// ```cpp
  /// class ExtractUserId: public FastFeature {
  ///   ...
  /// };
  /// ```
  clang::ast_matchers::MatchFinder match_finder_;

  /// 用于匹配 `TypeAliasDecl` 的 `MatchFinder`。
  ///
  /// 用于匹配模板类型。
  clang::ast_matchers::MatchFinder type_alias_finder_;

  /// 用于匹配 `InferFilterDecl` 的 `MatchFinder`。
  ///
  /// 用于匹配 `filter` 类。
  clang::ast_matchers::MatchFinder infer_filter_finder_;

  /// 用于在匹配上 `FeatureDecl` 后的处理。这是主要的逻辑处理类。
  FeatureDeclCallback feature_decl_callback_;

  /// 用于在匹配上 `TypeAliasDecl` 后的处理。
  TypeAliasCallback type_alias_callback_;

  /// 用于在匹配上 `InferFilterDecl` 后的处理。
  InferFilterCallback infer_filter_callback_;
};

/// 处理逻辑。
class ConvertAction : public clang::ASTFrontendAction {
 public:
  ConvertAction() {}
  ~ConvertAction() { delete rm_comment_; }

  /// 结束源文件处理后的动作，保存改写后的文件。
  void EndSourceFileAction() override;

  /// 处理特征抽取类。
  void handle_features();

  /// 处理 `filter` 类。
  void handle_infer_filters();

  /// 处理 `item_filter` 类。
  void handle_item_filters();

  /// 处理 `label_extractor` 类。
  void handle_label_extractor();

  /// 将改写的结果写入新的 `c++` 文件。
  void write_cc_file(const FeatureInfo &feature_info,
                     const std::string &new_h_filename,
                     const std::string &new_cc_filename,
                     const std::string &bs_extractor_name);

  /// 替换简单的字符串。
  ///
  /// 用于替换固定的字符串代码，不涉及到复杂的 `ast` 节点。
  ///
  /// 示例:
  /// ```cpp
  /// // 原始代码
  /// #include "teams/ad/ad_algorithm/feature/fast/frame/fast_feature.h";
  /// class ExtractUserId: public FastFeature {
  ///   ...
  /// };
  ///
  /// // 改写后的代码
  /// #include "teams/ad/ad_algorithm/bs_feature/fast/frame/bs_fast_feature.h";
  /// class BSExtractUserId: public BSFastFeature {
  ///   ...
  /// };
  /// ```
  std::string replace_simple(const std::string& content,
                             const std::string& class_name);

  /// 替换 `filter` 类。
  std::string replace_simple_infer_filter(const std::string& content);

  /// 插入新的字段定义。
  ///
  /// 用于处理复杂中间节点时，插入新的节点定义。
  std::string insert_new_field_def(const std::string& header_content,
                                   const FeatureInfo& feature_info);

  /// 创建 `ASTConsumer`。
  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& CI,
                                                        llvm::StringRef file) override;

 private:
  /// 用于改写 `c++` 代码的 `Rewriter`。
  clang::Rewriter rewriter_;

  /// 用于删除注释的处理器。
  RmComment* rm_comment_ = nullptr;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
