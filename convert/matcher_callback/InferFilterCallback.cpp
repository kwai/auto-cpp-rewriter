#include <unordered_set>

#include <absl/strings/str_join.h>
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclTemplate.h"

#include "../Tool.h"
#include "../Config.h"
#include "../info/FeatureInfo.h"
#include "../visitor/ExtractMethodVisitor.h"
#include "InferFilterCallback.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

void InferFilterCallback::run(const clang::ast_matchers::MatchFinder::MatchResult &Result) {
  auto config = GlobalConfig::Instance();

  if (const clang::CXXRecordDecl* cxx_record_decl =
      Result.Nodes.getNodeAs<clang::CXXRecordDecl>("InferFilter")) {
    if (config->dump_ast) {
      cxx_record_decl->dump();
    }

    std::string feature_name = "ItemFilter";
    FeatureInfo* feature_info_ptr = GlobalConfig::Instance()->feature_info_ptr(feature_name);
    if (feature_info_ptr == nullptr) {
      LOG(INFO) << "feature_info_ptr is nullptr! feature_name: " << feature_name;
      return;
    }

    LOG(INFO) << "find class, start process, feature_name: " << feature_name
              << ", is_template: " << cxx_record_decl->isTemplated()
              << ", template_common_info_int_values: "
              << absl::StrJoin(feature_info_ptr->template_common_info_values(), ",");

    std::string origin_file =
        Result.SourceManager->getFilename(cxx_record_decl->getBeginLoc()).str();

    feature_info_ptr->set_feature_name(feature_name);
    feature_info_ptr->set_origin_file(origin_file);
    feature_info_ptr->set_file_id(Result.SourceManager->getFileID(cxx_record_decl->getBeginLoc()));

    process_all_methods(cxx_record_decl, feature_info_ptr);
  }
}

void InferFilterCallback::process_all_methods(const clang::CXXRecordDecl* cxx_record_decl,
                                              FeatureInfo* feature_info_ptr) {
  for (auto it_method = cxx_record_decl->method_begin(); it_method != cxx_record_decl->method_end();
       it_method++) {
    ExtractMethodVisitor extract_method_visitor(rewriter_);
    extract_method_visitor.visit(*it_method, feature_info_ptr);
  }
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
