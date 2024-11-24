#include <glog/logging.h>
#include <sstream>

#include "clang/Basic/SourceLocation.h"

#include "../Env.h"
#include "../Tool.h"
#include "FieldDeclVisitor.h"
#include "../info/FeatureInfo.h"
#include "../handler/StrictRewriter.h"
#include "../handler/FieldDeclHandler.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

void FieldDeclVisitor::visit(const clang::FieldDecl* field_decl, FeatureInfo* feature_info_ptr) {
  if (field_decl == nullptr || feature_info_ptr == nullptr) {
    return;
  }

  feature_info_ptr->add_field_decl(field_decl);

  StrictRewriter strict_rewriter(rewriter_);
  FieldDeclHandler field_decl_handler(rewriter_);

  Env env;
  env.set_feature_info(feature_info_ptr);

  // 收集模板参数变量名
  if (field_decl->hasInClassInitializer()) {
    if (clang::Expr* init_expr = field_decl->getInClassInitializer()) {
      // 添加信息到 feature_info
      if (clang::InitListExpr* init_list_expr = dyn_cast<clang::InitListExpr>(init_expr)) {
        for (size_t i = 0; i < init_list_expr->getNumInits(); i++) {
          if (clang::DeclRefExpr* decl_ref_expr = dyn_cast<clang::DeclRefExpr>(init_list_expr->getInit(i))) {
            if (clang::ValueDecl* value_decl = decl_ref_expr->getDecl()) {
              if (value_decl->isTemplateParameter()) {
                LOG(INFO) << "add_template_var_name: " << stmt_to_string(decl_ref_expr);
                feature_info_ptr->add_template_var_name(stmt_to_string(decl_ref_expr));
              }
            }
          }
        }
      }

      // 处理改写逻辑
      recursive_visit(init_expr, &field_decl_handler, &env);

      // 替换类型中的 ItemType
      // 使用的时候可能会根据 int 值查找, 所以直接改成 int 类型。
      if (tool::is_map_item_type_int_type(field_decl->getType())) {
        std::ostringstream oss;
        oss << "std::unordered_map<int, int> "
            << field_decl->getNameAsString()
            << strict_rewriter.getRewrittenText(init_expr);

        strict_rewriter.ReplaceText(field_decl->getSourceRange(), oss.str());
      }
    }
  }

  clang::SourceRange source_range = field_decl->getSourceRange();
  std::string s = rewriter_.getRewrittenText(source_range);
  if (s.find("USED_FEATURES") != std::string::npos) {
    rewriter_.RemoveText(source_range);
  } else if (s.find("string") != std::string::npos) {
    // string a;
    // std::vector<string> arr;
    // std::map<string, int> m;
    rewriter_.ReplaceText(source_range, tool::fix_std_string(s));
  }
}

void FieldDeclVisitor::recursive_visit(clang::Stmt *stmt, FieldDeclHandler* handler_ptr, Env *env_ptr) {
  if (stmt == nullptr || handler_ptr == nullptr || env_ptr == nullptr) {
    return;
  }

  LOG(INFO) << "FieldDecl visit: " << stmt_to_string(stmt);

  if (clang::InitListExpr *init_list_expr = dyn_cast<clang::InitListExpr>(stmt)) {
    LOG(INFO) << "FieldDecl visit init_list_expr: " << stmt_to_string(stmt);
    for (size_t i = 0; i < init_list_expr->getNumInits(); i++) {
      recursive_visit(init_list_expr->getInit(i), handler_ptr, env_ptr);
    }
  }

  else if (clang::CXXStdInitializerListExpr* cxx_std_initializer_list_expr =
           dyn_cast<clang::CXXStdInitializerListExpr>(stmt)) {
    recursive_visit(cxx_std_initializer_list_expr->getSubExpr(), handler_ptr, env_ptr);
  }

  else if (clang::ExprWithCleanups* expr_with_cleanup = dyn_cast<clang::ExprWithCleanups>(stmt)) {
    recursive_visit(expr_with_cleanup->getSubExpr(), handler_ptr, env_ptr);
  }

  else if (clang::MaterializeTemporaryExpr *materialize_temporary_expr =
               dyn_cast<clang::MaterializeTemporaryExpr>(stmt)) {
    recursive_visit(materialize_temporary_expr->getSubExpr(), handler_ptr, env_ptr);
  }

  else if (clang::CXXConstructExpr *cxx_construct_expr = dyn_cast<clang::CXXConstructExpr>(stmt)) {
    for (size_t i = 0; i < cxx_construct_expr->getNumArgs(); i++) {
      recursive_visit(cxx_construct_expr->getArg(i), handler_ptr, env_ptr);
    }
  }

  else if (clang::DeclRefExpr *decl_ref_expr =
                 dyn_cast<clang::DeclRefExpr>(stmt)) {
    LOG(INFO) << "FieldDecl visit decl_ref_expr: " << stmt_to_string(stmt);
    handler_ptr->process(decl_ref_expr, env_ptr);
  }

  else if (clang::ConstantExpr *constant_expr =
               dyn_cast<clang::ConstantExpr>(stmt)) {
    LOG(INFO) << "FieldDecl visit constant_expr: " << stmt_to_string(stmt);
    recursive_visit(constant_expr->getSubExpr(), handler_ptr, env_ptr);
  }

  else {
    //
  }
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
