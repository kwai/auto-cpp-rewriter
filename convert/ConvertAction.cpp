#include <absl/strings/str_join.h>
#include <absl/strings/str_split.h>
#include <gflags/gflags.h>
#include <glog/logging.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>

// Declares clang::SyntaxOnlyAction.
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
// Declares llvm::cl::extrahelp.
#include "llvm/Support/CommandLine.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Lex/Lexer.h"

#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/Frontend/CompilerInstance.h"
#include "llvm/ADT/StringRef.h"

#include "Tool.h"
#include "info/FeatureInfo.h"
#include "ConvertAction.h"
#include "matcher_callback/InferFilterCallback.h"

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

bool RmComment::HandleComment(clang::Preprocessor& preprocessor,
                              clang::SourceRange source_range) {
  rewriter.ReplaceText(source_range, "");
  return false;
}

ConvertASTConsumer::ConvertASTConsumer(clang::Rewriter &R) :
  rewriter_(R),
  type_alias_callback_(R),
  feature_decl_callback_(R),
  infer_filter_callback_(R) {
  // 目前只能匹配到 typeAliasDecl(), 可能会有更好的匹配。
  auto TypeAliasMatcher = decl(typeAliasDecl(),
                                   namedDecl(matchesName("Extract.*"))).bind("TypeAlias");
  type_alias_finder_.addMatcher(TypeAliasMatcher, &type_alias_callback_);

  // TK_IgnoreUnlessSpelledInSource 用来忽略模板
  auto FeatureDeclMatcher = traverse(TK_IgnoreUnlessSpelledInSource,
                                     cxxRecordDecl(isDerivedFrom(hasName("FastFeature")),
                                                   unless(isExpandedFromMacro("DISALLOW_COPY_AND_ASSIGN")),
                                                   unless(isExpandedFromMacro("REGISTER_EXTRACTOR")))).bind("FeatureDecl");

  match_finder_.addMatcher(FeatureDeclMatcher, &feature_decl_callback_);

  // infer filter
  auto InferFilterMatcher = cxxRecordDecl(hasName("ItemFilter")).bind("InferFilter");
  infer_filter_finder_.addMatcher(InferFilterMatcher, &infer_filter_callback_);
}

void ConvertASTConsumer::HandleTranslationUnit(clang::ASTContext &Context) {
  // 解析模板参数, 这一步必须在解析 Extract 之前, 因为这些参数会被用到。
  type_alias_finder_.matchAST(Context);

  // 解析 Extract 逻辑并进行改写。
  match_finder_.matchAST(Context);

  // 处理 infer filter
  infer_filter_finder_.matchAST(Context);
}

void ConvertAction::EndSourceFileAction() {
  // 按顺序执行各个处理逻辑。
  handle_features();
  handle_infer_filters();
  handle_item_filters();
  handle_label_extractor();
}

void ConvertAction::handle_features() {
  auto config = GlobalConfig::Instance();
  {
    std::lock_guard<std::mutex> lock(config->mu);

    // 处理特征抽取类，依次执行处理逻辑。
    std::vector<std::string> paths;
    for (auto it = config->feature_info.begin(); it != config->feature_info.end(); it++) {
      const std::string& extractor_name = it->first;

      // 跳过 `ItemFilter` 类。
      if (extractor_name == "ItemFilter") {
        continue;
      }

      std::string bs_extractor_name = std::string("BS") + extractor_name;
      const FeatureInfo& feature_info = it->second;

      const std::string& origin_file = feature_info.origin_file();
      if (origin_file.size() == 0) {
        LOG(INFO) << "origin_file is empty! feature_name: " << extractor_name;
        continue;
      }

      // 跳过已经改写过的文件。
      if (tool::is_bs_already_rewritten(origin_file)) {
        if (!config->overwrite) {
          LOG(INFO) << tool::get_bs_correspond_path(origin_file) << "already exists, skip";
          continue;
        }
      }

      paths.push_back(feature_info.origin_file());

      // 读取原始文件内容。
      const clang::FileID& file_id = feature_info.file_id();
      std::string header_content;
      llvm::raw_string_ostream raw_string(header_content);

      // 获取改写后的内容。
      rewriter_.getEditBuffer(file_id).write(raw_string);

      // 替换简单的字符串。
      header_content = replace_simple(header_content, extractor_name);

      std::string new_h_filename = tool::get_bs_correspond_path(origin_file);

      for (auto& path : paths) {
        auto pos = header_content.find(path);
        if (pos != std::string::npos) {
          std::string new_path = tool::get_bs_correspond_path(origin_file);
          header_content.replace(pos, path.size(), new_path);
        }
      }

      // format
      std::string cmd_format(
          "clang-format "
          "--style=\"{BasedOnStyle: Google, ColumnLimit: 110, IndentCaseLabels: true}\" -i ");  // NOLINT

      // 写入到 .h 文件
      std::error_code ec;
      std::ofstream wfile(new_h_filename.c_str());
      if (wfile.is_open()) {
        wfile << header_content;
      }
      wfile.close();
      std::system((cmd_format + new_h_filename).c_str());
      LOG(INFO) << "convert done,  .h: " << new_h_filename;

      // 写入到 .cc 文件
      if (!feature_info.is_template()) {
        std::string new_cc_filename = std::regex_replace(new_h_filename, std::regex("\\.h"), ".cc");
        write_cc_file(feature_info, new_h_filename, new_cc_filename, bs_extractor_name);
        std::system((cmd_format + new_cc_filename).c_str());
        LOG(INFO) << "convert done, .cc: " << new_cc_filename;
      }
    }

    LOG(INFO) << "start write bs field";
    json field_output = json::object();
    for (auto it_feature = config->feature_info.begin(); it_feature != config->feature_info.end();
         it_feature++) {
      const std::string& extractor_name = it_feature->first;
      FeatureInfo& feature_info = it_feature->second;

      feature_info.gen_output();
      field_output[extractor_name] = feature_info.output();
    }

    if (config->field_detail_filename.size() > 0) {
      std::ofstream out_bs_fields(std::string("../data/") + config->field_detail_filename);
      out_bs_fields << field_output.dump(4);
      out_bs_fields.close();
      LOG(INFO) << "write field to file: data/" << config->field_detail_filename;
    } else {
      LOG(INFO) << "field_detail_filename is empty!";
    }

    LOG(INFO) << "done";
  }
}

void ConvertAction::handle_infer_filters() {
  auto config = GlobalConfig::Instance();
  {
    std::lock_guard<std::mutex> lock(config->mu);
    std::vector<std::string> paths;
    for (auto it = config->feature_info.begin(); it != config->feature_info.end(); it++) {
      const std::string& extractor_name = it->first;
      if (extractor_name != "ItemFilter") {
        continue;
      }

      const FeatureInfo& feature_info = it->second;

      const std::string& origin_file = feature_info.origin_file();
      if (origin_file.size() == 0) {
        LOG(INFO) << "origin_file is empty! class_name: " << extractor_name;
        continue;
      }

      const clang::FileID& file_id = feature_info.file_id();
      std::string header_content;
      llvm::raw_string_ostream raw_string(header_content);
      rewriter_.getEditBuffer(file_id).write(raw_string);
      header_content = replace_simple_infer_filter(header_content);

      std::string new_h_filename = "teams/ad/ad_nn/utils/bs_item_filter_auto.h";
      std::string new_cc_filename = "teams/ad/ad_nn/utils/bs_item_filter_auto.cc";

      // format
      std::string cmd_format(
          "clang-format "
          "--style=\"{BasedOnStyle: Google, ColumnLimit: 110, IndentCaseLabels: true}\" -i ");

      // 写入到 .h 文件
      std::error_code ec;
      std::ofstream wfile(new_h_filename.c_str());
      if (wfile.is_open()) {
        wfile << header_content;
      }
      wfile.close();
      std::system((cmd_format + new_h_filename).c_str());
      LOG(INFO) << "convert done,  .h: " << new_h_filename;

      // 写入到 .cc 文件
      std::ofstream wfile_cc(new_cc_filename.c_str());
      if (wfile_cc.is_open()) {
        wfile_cc << "#include \"teams/ad/ad_nn/utils/bs_item_filter.h\"\n\n";
        wfile_cc << "namespace ks {\nnamespace ad_nn {\n\n";

        auto& infer_filter_funcs = config->infer_filter_funcs;
        for (auto it_filter = infer_filter_funcs.begin(); it_filter != infer_filter_funcs.end();
             it_filter++) {
          wfile_cc << "bool BSItemFilter::" << it_filter->first
                   << "(const SampleInterface& bs, const FilterCondition& filter_condition, size_t pos) "
                   << replace_simple_infer_filter(it_filter->second)
                   << "\n\n";
        }

        wfile_cc << "}  // namespace ad_nn\n}  // namespace ks";
      }
      wfile_cc.close();
      std::system((cmd_format + new_cc_filename).c_str());
      LOG(INFO) << "convert done,  .cc: " << new_cc_filename;
    }
    LOG(INFO) << "done";
  }
}

void ConvertAction::handle_item_filters() {}

void ConvertAction::handle_label_extractor() {}

void ConvertAction::write_cc_file(const FeatureInfo& feature_info,
                                  const std::string& new_h_filename,
                                  const std::string& new_cc_filename,
                                  const std::string& bs_extractor_name) {
  std::ofstream wfile_cc(new_cc_filename.c_str());

  const std::string &extract_method_content = feature_info.extract_method_content();

  if (wfile_cc.is_open()) {
    // 写入常见的头文件。
    if (feature_info.has_hash_fn_str()) {
      wfile_cc << "#include \"teams/ad/ad_nn/bs_field_helper/bs_field_helper.h\"\n";
    }

    if (feature_info.has_query_token()) {
      wfile_cc << "#include \"teams/ad/ad_algorithm/bs_feature/fast/frame/bs_action_util.h\"\n";
    }

    if (extract_method_content.find("std::move") != std::string::npos) {
      wfile_cc << "#include <utility>\n";
    }

    if (extract_method_content.find("unordered_map") != std::string::npos) {
      wfile_cc << "#include <unordered_map>\n";
    }

    if (extract_method_content.find("unordered_set") != std::string::npos) {
      wfile_cc << "#include <unordered_set>\n";
    }

    wfile_cc << "#include \"" << new_h_filename << "\"\n\n";
    wfile_cc << "namespace ks {\nnamespace ad_algorithm {\n"
             << bs_extractor_name << "::" << bs_extractor_name << "(): BS"
             << feature_info.constructor_info().init_list() << "\n"
             << tool::rm_empty_line(feature_info.constructor_info().body_content())
             << "\n\n";

    // `reco_user_info` 相关字段特殊处理，需要通过 `gflags` 参数区分逻辑。
    if (const auto& reco_extract_body = feature_info.reco_extract_body()) {
      wfile_cc << "void " << bs_extractor_name << "::ExtractWithBSRecoUserInfo("
               << "const BSLog& bslog, size_t pos, std::vector<ExtractResult>* result) \n"
               << tool::rm_empty_line(reco_extract_body.value()) << "\n";
    }

    // 写入主要的 `Extract` 方法。
    wfile_cc << "void " << bs_extractor_name << "::Extract("
             << "const BSLog& bslog, size_t pos, std::vector<ExtractResult>* "
                "result) \n"
             << tool::rm_empty_line(extract_method_content) << "\n";

    // 写入其他函数。
    const auto &other_methods = feature_info.other_methods();
    for (auto it_method = other_methods.begin(); it_method != other_methods.end(); it_method++) {
      wfile_cc << it_method->second.bs_return_type() << " "
               << "BS" << feature_info.feature_name()
               << "::" << it_method->second.decl() << it_method->second.body()
               << "\n";
    }

    wfile_cc << "}  // namespace ad_algorithm\n}  // namespace ks\n";
  }

  wfile_cc.close();
}

std::string ConvertAction::replace_simple(const std::string& content,
                                          const std::string& class_name) {
  std::regex p_class_name(class_name);

  static std::regex p_more_class("class (Extract.*) ?: ?public");
  static std::regex p_empty_line("\n *;");
  static std::regex p_colon(": *;");
  static std::regex p_adlog("const AdLog ?\\& ?(adlog|ad_log)");
  static std::regex p_fast_feature("FastFeature");
  static std::regex p_register("REGISTER_EXTRACTOR");
  static std::regex p_register_seq("REGISTER_SEQUENCE_EXTRACTOR");
  static std::regex p_using("using Extract");
  static std::regex p_extract_bs("EXTRACTOR\\(Extract");
  static std::regex p_ad_callback_log("(::)?auto_cpp_rewriter::AdCallbackLog");
  static std::regex p_name_extend_two("(::)?auto_cpp_rewriter::CommonInfoAttr");
  // 必须放到 FastFeature 替换以后
  static std::regex p_extract_multi("ExtractMultiAttrBSFastFeatureNoPrefix");
  static std::regex p_tempalte_item_type("template ?<ItemType");

  std::string s = std::regex_replace(content, p_class_name, std::string("BS") + class_name);
  s = std::regex_replace(s, p_more_class, "class BS$1: public");
  s = std::regex_replace(s, p_empty_line, "");
  s = std::regex_replace(s, p_colon, ";");
  s = std::regex_replace(s, p_adlog, "const BSLog& bslog");
  s = std::regex_replace(s, p_fast_feature, "BSFastFeature");
  s = std::regex_replace(s, p_register, "REGISTER_BS_EXTRACTOR");
  s = std::regex_replace(s, p_register_seq, "REGISTER_BS_SEQUENCE_EXTRACTOR");
  s = std::regex_replace(s, p_using, "using BSExtract");
  s = std::regex_replace(s, p_extract_bs, "EXTRACTOR(BSExtract");
  s = std::regex_replace(s, p_ad_callback_log, "::bs::auto_cpp_rewriter::AdCallbackLog");
  s = std::regex_replace(s, p_name_extend_two, "::bs::auto_cpp_rewriter::CommonInfoAttr");
  s = std::regex_replace(s, p_tempalte_item_type, "template<bs::ItemType");

  if (s.find("ad::CommonInfoAttr") != std::string::npos) {
    std::string last_include = tool::find_last_include(s);
  }

  s = std::regex_replace(s, p_extract_multi, "BSExtractMultiAttrFastFeatureNoPrefix");

  s = tool::fix_std_string(s);

  return s;
}

std::string ConvertAction::replace_simple_infer_filter(const std::string& content) {
  static std::regex p_item_filter("ItemFilter");
  static std::regex p_enum_item("BSFieldEnum::item");
  static std::regex p_bs("\\(\\*bs");
  static std::regex p_static_inline("static inline bool");

  std::string s = std::regex_replace(content, p_item_filter, std::string("BSItemFilter"));
  s = std::regex_replace(s, p_enum_item, std::string("BSFieldEnum::adlog_item"));
  s = std::regex_replace(s, p_bs, std::string("(bs"));
  s = std::regex_replace(s, p_static_inline, std::string("static bool"));

  return s;
}

std::string ConvertAction::insert_new_field_def(const std::string& header_content,
                                                const FeatureInfo& feature_info) {
  static std::regex p_private("private ?:");
  std::smatch m;

  std::ostringstream oss;
  oss << "private:\n  ";

  const std::unordered_map<std::string, NewVarDef>& new_field_defs = feature_info.new_field_defs();
  for (auto it = new_field_defs.begin(); it != new_field_defs.end(); it++) {
    if (it->second.var_def().size() > 0) {
      oss << it->second.var_def() << ";\n";
    }
    if (it->second.exists_var_def().size() > 0) {
      oss << it->second.exists_var_def() << ";\n";
    }
  }

  std::string s = std::regex_replace(header_content,
                                     p_private,
                                     oss.str(),
                                     std::regex_constants::format_first_only);

  return s;
}

std::unique_ptr<clang::ASTConsumer> ConvertAction::CreateASTConsumer(clang::CompilerInstance &CI,
                                                                     llvm::StringRef file) {
  rewriter_.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());

  LOG(INFO) << "remove_comment: " << GlobalConfig::Instance()->remove_comment;
  if (GlobalConfig::Instance()->remove_comment) {
    rm_comment_ = new RmComment(rewriter_);
    LOG(INFO) << "add comment handler";
    CI.getPreprocessor().addCommentHandler(rm_comment_);
  }

  return std::make_unique<ConvertASTConsumer>(rewriter_);
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
