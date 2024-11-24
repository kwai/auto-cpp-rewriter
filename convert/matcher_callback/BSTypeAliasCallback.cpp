#include <absl/strings/str_join.h>

#include <unordered_set>

#include "clang/AST/Decl.h"

#include "../Env.h"
#include "../Tool.h"
#include "../Config.h"
#include "../ExprInfo.h"
#include "../ExprParser.h"
#include "../info/FeatureInfo.h"
#include "BSTypeAliasCallback.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

void BSTypeAliasCallback::run(const clang::ast_matchers::MatchFinder::MatchResult &Result) {
  auto config = GlobalConfig::Instance();

  if (const clang::Decl* decl = Result.Nodes.getNodeAs<clang::Decl>("BSTypeAlias")) {
    if (const clang::TypeAliasDecl* type_alias_decl = dyn_cast<clang::TypeAliasDecl>(decl)) {
      if (const clang::TypedefNameDecl* typedef_name_decl = type_alias_decl->getMostRecentDecl()) {
        clang::QualType qual_type = typedef_name_decl->getUnderlyingType();
        if (starts_with(qual_type.getAsString(), "BSExtract")) {
          if (config->dump_ast) {
            decl->dump();
          }

          if (const clang::TemplateSpecializationType* tmpl_type =
              dyn_cast<clang::TemplateSpecializationType>(qual_type.getTypePtr())) {
            clang::TemplateName template_name = tmpl_type->getTemplateName();

            if (template_name.getKind() == clang::TemplateName::NameKind::Template) {
              if (clang::TemplateDecl* template_decl = template_name.getAsTemplateDecl()) {
                std::string feature_name = template_decl->getNameAsString();
                FeatureInfo* feature_info_ptr = GlobalConfig::Instance()->feature_info_ptr(feature_name);
                if (feature_info_ptr == nullptr) {
                  LOG(INFO) << "feature_info_ptr is nullptr!";
                  return;
                }

                auto template_arguments = tmpl_type->template_arguments();

                for (size_t i = 0; i < template_arguments.size(); i++) {
                  const clang::TemplateArgument& arg = template_arguments[i];

                  process_template_param(arg);

                  if (arg.getKind() == clang::TemplateArgument::ArgKind::Expression) {
                    clang::Expr* expr = arg.getAsExpr();
                    if (expr != nullptr && tool::is_common_info_enum(expr->getType())) {
                      if (absl::optional<int> int_value = find_common_attr_int_value(expr)) {
                        LOG(INFO) << "find common info enum in template param, class: " << feature_name
                                  << ", expr: " << stmt_to_string(expr)
                                  << ", int_value: " << *int_value;
                        feature_info_ptr->add_template_common_info_value(*int_value);
                      }
                    }
                  } else if (arg.getKind() == clang::TemplateArgument::ArgKind::Type) {
                    // 暂时忽略
                  }
                }

                LOG(INFO) << "find template_common_info_int_values: "
                          << absl::StrJoin(feature_info_ptr->template_common_info_values(), ",");
              }
            }
          }
        }
      }
    }
  }
}

void BSTypeAliasCallback::process_template_param(const clang::TemplateArgument& arg) {
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
