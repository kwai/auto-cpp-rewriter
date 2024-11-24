#pragma once

#include <glog/logging.h>
#include <absl/types/optional.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <iostream>
#include <list>
#include <mutex>
#include <set>
#include <sstream>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "info/FeatureInfo.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

using nlohmann::json;

class FeatureInfo;

class GlobalConfig final {
 public:
  static GlobalConfig* Instance() {
    static GlobalConfig instance;
    return &instance;
  }

  FeatureInfo* feature_info_ptr(const std::string& feature_name);

  std::mutex mu;
  clang::FileID file_id;
  clang::SourceManager* source_manager = nullptr;

  std::string cmd = "";
  bool remove_comment = false;
  bool dump_ast = false;
  bool overwrite = false;
  std::string filename;
  std::string message_def_filename;
  std::string field_detail_filename;
  bool use_reco_user_info = false;
  bool rewrite_reco_user_info = false;

  std::string middle_node_json_file = "data/middle_node.json";

  json all_adlog_fields = json::object();
  json feature_def = json::object();
  json fast_feature_def = json::object();
  std::map<std::string, int> enum_map;

  std::vector<std::string> filenames;
  std::string feature_list_filename =
    "teams/ad/ad_algorithm/feature/fast/impl/feature_list_complete_adlog.cc";
  std::map<std::string, std::string> feature_filename_map;
  std::map<std::string, std::string> feature_content_map;
  std::unordered_map<std::string, FeatureInfo> feature_info;

  /// infer filter 函数实现，暂时先用 map 简单处理，后面再重构。
  std::unordered_map<std::string, std::string> infer_filter_funcs;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
