#include <unordered_set>

#include <absl/strings/str_join.h>
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclTemplate.h"

#include "../Tool.h"
#include "../Config.h"
#include "../info/FeatureInfo.h"
#include "../visitor/CtorVisitor.h"
#include "../visitor/FieldDeclVisitor.h"
#include "../visitor/ExtractMethodVisitor.h"
#include "./FeatureDeclCallback.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

void FeatureDeclCallback::run(const clang::ast_matchers::MatchFinder::MatchResult &Result) {
  auto config = GlobalConfig::Instance();

  if (const clang::CXXRecordDecl* cxx_record_decl = Result.Nodes.getNodeAs<clang::CXXRecordDecl>("FeatureDecl")) {
    std::string feature_name = cxx_record_decl->getNameAsString();
    if (tool::is_skip(feature_name)) {
      LOG(INFO) << "feature_name: " << feature_name << ", skip";
      return;
    }

    if (config->dump_ast) {
      cxx_record_decl->dump();
    }

    FeatureInfo* feature_info_ptr = GlobalConfig::Instance()->feature_info_ptr(feature_name);
    if (feature_info_ptr == nullptr) {
      LOG(INFO) << "feature_info_ptr is nullptr! feature_name: " << feature_name;
      return;
    }

    LOG(INFO) << "find class, start process, feature_name: " << feature_name
              << ", is_template: " << cxx_record_decl->isTemplated()
              << ", template_common_info_int_values: "
              << absl::StrJoin(feature_info_ptr->template_common_info_values(), ",");

    std::string origin_file = Result.SourceManager->getFilename(cxx_record_decl->getBeginLoc()).str();

    feature_info_ptr->set_feature_name(feature_name);
    feature_info_ptr->set_origin_file(origin_file);
    feature_info_ptr->set_is_template(cxx_record_decl->isTemplated());
    feature_info_ptr->set_file_id(Result.SourceManager->getFileID(cxx_record_decl->getBeginLoc()));
    feature_info_ptr->set_has_cc_file(tool::has_cc_file(origin_file));

    if (cxx_record_decl->isTemplated()) {
      process_template_params(cxx_record_decl, feature_info_ptr);
    }

    std::string origin_buffer;;
    llvm::raw_string_ostream raw_string(origin_buffer);
    rewriter_.getEditBuffer(feature_info_ptr->file_id()).write(raw_string);
    feature_info_ptr->set_origin_buffer(origin_buffer);

    // 必须按照 field, ctor, method 的顺序
    for (auto it_field = cxx_record_decl->field_begin(); it_field != cxx_record_decl->field_end(); it_field++) {
      process_field(*it_field, feature_info_ptr);
    }

    for (auto it_ctor = cxx_record_decl->ctor_begin(); it_ctor != cxx_record_decl->ctor_end(); it_ctor++) {
      process_ctor(*it_ctor, feature_info_ptr);
    }

    process_all_methods(cxx_record_decl, feature_info_ptr);
  }
}

void FeatureDeclCallback::process_ctor(clang::CXXConstructorDecl* cxx_constructor_decl,
                                       FeatureInfo* feature_info_ptr) {
  CtorVisitor ctor_visitor(rewriter_);
  ctor_visitor.visit(cxx_constructor_decl, feature_info_ptr);
}

void FeatureDeclCallback::process_field(clang::FieldDecl* field_decl,
                                        FeatureInfo* feature_info_ptr) {
  FieldDeclVisitor field_decl_visitor(rewriter_);
  field_decl_visitor.visit(field_decl, feature_info_ptr);
}

// 先访问其他方法，再访问 `Extract`
void FeatureDeclCallback::process_all_methods(const clang::CXXRecordDecl* cxx_record_decl,
                                              FeatureInfo* feature_info_ptr) {
  for (auto it_method = cxx_record_decl->method_begin(); it_method != cxx_record_decl->method_end(); it_method++) {
    if ((*it_method)->getNameAsString() != EXTRACT) {
      process_method(*it_method, feature_info_ptr);
    }
  }

  for (auto it_method = cxx_record_decl->method_begin(); it_method != cxx_record_decl->method_end(); it_method++) {
    if ((*it_method)->getNameAsString() == EXTRACT) {
      process_method(*it_method, feature_info_ptr);
    }
  }
}

void FeatureDeclCallback::process_method(clang::CXXMethodDecl* cxx_method_decl,
                                        FeatureInfo* feature_info_ptr) {
  if (cxx_method_decl == nullptr || feature_info_ptr == nullptr) {
    LOG(INFO) << "cxx_method_decl or feature_info_ptr is nullptr!";
    return;
  }

  ExtractMethodVisitor extract_method_visitor(rewriter_);
  extract_method_visitor.visit(cxx_method_decl, feature_info_ptr);
}

void FeatureDeclCallback::process_template_params(const clang::CXXRecordDecl* cxx_record_decl,
                                                  FeatureInfo* feature_info_ptr) {
  if (cxx_record_decl == nullptr || feature_info_ptr == nullptr) {
    return;
  }

  if (clang::ClassTemplateDecl* class_template_decl = cxx_record_decl->getDescribedClassTemplate()) {
    if (clang::TemplateParameterList* template_parameter_list = class_template_decl->getTemplateParameters()) {
      std::vector<std::string> param_names(template_parameter_list->size());
      for (size_t i = 0; i < template_parameter_list->size(); i++) {
        if (clang::NamedDecl* param = template_parameter_list->getParam(i)) {
          param_names[i] = param->getNameAsString();
        }
      }

      feature_info_ptr->set_template_param_names(param_names);
    }
  }
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
