#include <absl/strings/str_join.h>

#include <unordered_set>

#include "clang/AST/Decl.h"

#include "../Env.h"
#include "../Tool.h"
#include "../Config.h"
#include "../ExprInfo.h"
#include "../ExprParser.h"
#include "../info/FeatureInfo.h"
#include "../info/TemplateParamInfo.h"
#include "TypeAliasCallback.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

void TypeAliasCallback::run(const clang::ast_matchers::MatchFinder::MatchResult &Result) {
  auto config = GlobalConfig::Instance();

  if (const clang::Decl* decl = Result.Nodes.getNodeAs<clang::Decl>("TypeAlias")) {
    if (const clang::TypeAliasDecl* type_alias_decl = dyn_cast<clang::TypeAliasDecl>(decl)) {
      if (const clang::TypedefNameDecl* typedef_name_decl = type_alias_decl->getMostRecentDecl()) {
        clang::QualType qual_type = typedef_name_decl->getUnderlyingType();
        if (starts_with(qual_type.getAsString(), "Extract")) {
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

                std::string special_name = typedef_name_decl->getNameAsString();
                feature_info_ptr->add_specialization_class(special_name);

                auto template_arguments = tmpl_type->template_arguments();

                for (size_t i = 0; i < template_arguments.size(); i++) {
                  const clang::TemplateArgument& arg = template_arguments[i];

                  process_template_param(arg);

                  if (arg.getKind() == clang::TemplateArgument::ArgKind::Expression) {
                    clang::Expr* expr = arg.getAsExpr();
                    TemplateParamInfo* param_ptr =
                      feature_info_ptr->touch_template_param_ptr(special_name, i);
                    if (param_ptr == nullptr) {
                      LOG(INFO) << "param_ptr is nullptr! name: " << special_name << ", index: " << i;
                      continue;
                    }

                    param_ptr->set_value_str(stmt_to_string(expr));
                    if (expr != nullptr) {
                      param_ptr->set_qual_type(expr->getType());

                      if (tool::is_common_info_enum(expr->getType())) {
                        if (absl::optional<int> int_value = find_common_attr_int_value(expr)) {
                          LOG(INFO) << "find common info enum in template param, class: " << feature_name
                                    << ", expr: " << stmt_to_string(expr) << ", int_value: " << *int_value;
                          param_ptr->set_enum_value(*int_value);
                        }
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

void TypeAliasCallback::process_template_param(const clang::TemplateArgument& arg) {
  Env env;

  if (arg.getKind() == clang::TemplateArgument::ArgKind::Expression) {
    clang::Expr *expr = arg.getAsExpr();
    if (expr == nullptr) {
      return;
    }

    auto expr_info_ptr = parse_expr(expr, &env);
    if (expr_info_ptr == nullptr) {
      return;
    }

    if (expr_info_ptr->is_item_type_enum()) {
      std::string new_text = std::string("bs::ItemType::") + expr_info_ptr->get_ad_enum_name();
      LOG(INFO) << "find template ad enum param: " << expr_info_ptr->origin_expr_str()
                << ", type: " << expr->getType().getAsString()
                << ", replace: " << new_text;
      rewriter_.ReplaceText(expr, new_text);
    }
  }
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
