#include <glog/logging.h>
#include <absl/strings/match.h>
#include <absl/strings/str_split.h>

#include "../Env.h"
#include "../Tool.h"
#include "./BSCtorVisitor.h"
#include "../info/ConstructorInfo.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

void BSCtorVisitor::visit(clang::CXXConstructorDecl* cxx_constructor_decl, FeatureInfo* feature_info_ptr) {
  if (cxx_constructor_decl == nullptr || feature_info_ptr == nullptr) {
    return;
  }

  std::string feature_type;
  const std::string& feature_name = feature_info_ptr->feature_name();

  ConstructorInfo* ctor_info_ptr = &(feature_info_ptr->mutable_constructor_info());
  if (ctor_info_ptr == nullptr) {
    LOG(INFO) << "cannot get constructor_info from feature_info! feature_name: "
              << feature_info_ptr->feature_name();
    return;
  }

  // 处理构造函数参数, 获取 feature_type
  for (clang::CXXCtorInitializer *const * it = cxx_constructor_decl->init_begin(); it != cxx_constructor_decl->init_end(); it++) {
    if (ctor_info_ptr->init_list().size() == 0) {
      ctor_info_ptr->set_init_list(rewriter_.getRewrittenText((*it)->getSourceRange()));
      ctor_info_ptr->set_first_init_stmt(const_cast<clang::CXXCtorInitializer*>(*it));
    }

    if (clang::CXXConstructExpr* base_init = dyn_cast<clang::CXXConstructExpr>((*it)->getInit())) {
      LOG(INFO) << "find base_init";
      if (base_init->getNumArgs() > 0) {
        feature_type = stmt_to_string(base_init->getArg(0));
      }
      break;
    } else if (clang::ParenListExpr* base_init = dyn_cast<clang::ParenListExpr>((*it)->getInit())) {
      LOG(INFO) << "find base_init paren list expr";
      if (base_init->getNumExprs() > 0) {
        feature_type = stmt_to_string(base_init->getExpr(0));
      }
      break;
    }
  }

  // 可能是别的构造函数，跳过。
  // 用户指定的构造函数必须有 feature_type。
  // if (feature_type.size() == 0) {
  //  return;
  // }

  ctor_info_ptr->set_feature_type(feature_type);
  feature_info_ptr->set_feature_type(feature_type);

  std::string content = rewriter_.getRewrittenText(cxx_constructor_decl->getSourceRange());
  if (content.size() > 0 && ctor_info_ptr->params().size() == 0) {
    ctor_info_ptr->set_source_range(cxx_constructor_decl->getSourceRange());

    // 可能会有多个构造函数，需要区分下，去掉拷贝构造以及移动构造等。
    for (size_t i = 0; i < cxx_constructor_decl->getNumParams(); i++) {
      std::string param_str =
          rewriter_.getRewrittenText(cxx_constructor_decl->getParamDecl(i)->getSourceRange());
      if (param_str.size() > 0) {
        LOG(INFO) << "add ctor params: " << param_str << ", i: " << i;
        ctor_info_ptr->add_param(param_str);
      }
    }
  }

  // 处理构造函数逻辑, 获取 enum 等变量
  if (cxx_constructor_decl->hasBody()) {
    ctor_info_ptr->set_body(cxx_constructor_decl->getBody());
    ctor_info_ptr->set_body_end(cxx_constructor_decl->getBody()->getEndLoc());
    Env env;
    std::vector<std::string> fields;
    if (clang::Stmt* body = cxx_constructor_decl->getBody()) {
      recursive_visit(body, feature_info_ptr, &env, &fields);
    }

    LOG(INFO) << "fields: " << absl::StrJoin(fields, ", ");

    VarDeclInfo& var_decl_info = ctor_info_ptr->mutable_var_decl_info();
    var_decl_info.update(env.var_decls());
    LOG(INFO) << "env.var_decls.size: " << env.var_decls().size()
              << ", var_decl_info.var_decls.size: " << var_decl_info.var_decls().size();
  }
}

void BSCtorVisitor::recursive_visit(clang::Stmt* stmt,
                                    FeatureInfo* info_ptr,
                                    Env* env_ptr,
                                    std::vector<std::string>* fields_ptr) {
  if (stmt == nullptr || info_ptr == nullptr || env_ptr == nullptr || fields_ptr == nullptr) {
    return;
  }

  if (clang::CompoundStmt* compound_stmt = dyn_cast<clang::CompoundStmt>(stmt)) {
    for (clang::CompoundStmt::body_iterator start = compound_stmt->body_begin();
         start != compound_stmt->body_end(); start++) {
      recursive_visit(*start, info_ptr, env_ptr, fields_ptr);
    }

  } else if (clang::DeclStmt* decl_stmt = dyn_cast<clang::DeclStmt>(stmt)) {
    env_ptr->update(decl_stmt);

  } else if (clang::ExprWithCleanups* expr_with_cleanups = dyn_cast<clang::ExprWithCleanups>(stmt)) {
    recursive_visit(expr_with_cleanups->getSubExpr(), info_ptr, env_ptr, fields_ptr);

  } else if (clang::ImplicitCastExpr* implicit_cast_expr = dyn_cast<clang::ImplicitCastExpr>(stmt)) {
    recursive_visit(implicit_cast_expr->getSubExpr(), info_ptr, env_ptr, fields_ptr);

  } else if (clang::CXXMemberCallExpr* cxx_member_call_expr = dyn_cast<clang::CXXMemberCallExpr>(stmt)) {
    for (unsigned i = 0; i < cxx_member_call_expr->getNumArgs(); i++) {
      recursive_visit(cxx_member_call_expr->getArg(i), info_ptr, env_ptr, fields_ptr);
    }
    process(cxx_member_call_expr, info_ptr, env_ptr, fields_ptr);

  } else if (clang::CXXOperatorCallExpr* cxx_operator_call_expr = dyn_cast<clang::CXXOperatorCallExpr>(stmt)) {
    for (size_t i = 0; i < cxx_operator_call_expr->getNumArgs(); i++) {
      recursive_visit(cxx_operator_call_expr->getArg(i), info_ptr, env_ptr, fields_ptr);
    }

  } else if (clang::UnaryOperator* unary_operator = dyn_cast<clang::UnaryOperator>(stmt)) {
    recursive_visit(unary_operator->getSubExpr(), info_ptr, env_ptr, fields_ptr);

  } else if (clang::MemberExpr* member_expr = dyn_cast<clang::MemberExpr>(stmt)) {

  } else if (clang::BinaryOperator* binary_operator = dyn_cast<clang::BinaryOperator>(stmt)) {
    env_ptr->update(binary_operator);
    recursive_visit(binary_operator->getLHS(), info_ptr, env_ptr, fields_ptr);
    recursive_visit(binary_operator->getRHS(), info_ptr, env_ptr, fields_ptr);
    env_ptr->clear_binary_op_info();

  } else if (clang::MaterializeTemporaryExpr* materialize_temporary_expr = dyn_cast<clang::MaterializeTemporaryExpr>(stmt)) {
    recursive_visit(materialize_temporary_expr->getSubExpr(), info_ptr, env_ptr, fields_ptr);

  } else if (clang::CXXConstructExpr* cxx_construct_expr = dyn_cast<clang::CXXConstructExpr>(stmt)) {
    for (size_t i = 0; i < cxx_construct_expr->getNumArgs(); i++) {
      recursive_visit(cxx_construct_expr->getArg(i), info_ptr, env_ptr, fields_ptr);
    }

  } else if (clang::IfStmt* if_stmt = dyn_cast<clang::IfStmt>(stmt)) {
    absl::optional<IfInfo> tmp_if_info;
    tmp_if_info.emplace(if_stmt);
    Env new_env(env_ptr);
    new_env.update(if_stmt);

    clang::Expr* cond_expr = if_stmt->getCond();
    clang::Stmt* then_expr = if_stmt->getThen();
    clang::Stmt* else_expr = if_stmt->getElse();

    recursive_visit(cond_expr->IgnoreImpCasts(), info_ptr, &new_env, fields_ptr);
    recursive_visit(then_expr, info_ptr, &new_env, fields_ptr);
    recursive_visit(else_expr, info_ptr, &new_env, fields_ptr);

  } else if (clang::ForStmt* for_stmt = dyn_cast<clang::ForStmt>(stmt)) {
    for (auto it = for_stmt->child_begin(); it != for_stmt->child_end(); it++) {
      recursive_visit(*it, info_ptr, env_ptr, fields_ptr);
    }

  } else if (clang::CXXForRangeStmt* cxx_for_range_stmt = dyn_cast<clang::CXXForRangeStmt>(stmt)) {
    for (auto it = cxx_for_range_stmt->child_begin(); it != cxx_for_range_stmt->child_end(); it++) {
      recursive_visit(*it, info_ptr, env_ptr, fields_ptr);
    }

  } else if (clang::DeclRefExpr* decl_ref_expr = dyn_cast<clang::DeclRefExpr>(stmt)) {
    if (tool::is_common_info_enum(decl_ref_expr->getType())) {
      ConstructorInfo* ctor_info_ptr = &(info_ptr->mutable_constructor_info());

      if (const auto& if_info = env_ptr->cur_if_info()) {
        if (if_info->if_stmt() != nullptr && if_info->if_stmt()->getThen() != nullptr) {
          ctor_info_ptr->add_common_info_enum(decl_ref_expr, if_info->if_stmt()->getThen()->getEndLoc());
          LOG(INFO) << "ctor add common_info_enum: " << stmt_to_string(decl_ref_expr);
        } else {
          LOG(INFO) << "if_stmt is nullptr!";
        }
      } else {
        ctor_info_ptr->add_common_info_enum(decl_ref_expr);
      }
    }

  } else {
    LOG(INFO) << "unsupported stmt, trated as string: " << stmt_to_string(stmt);
  }
}

void BSCtorVisitor::process(clang::CXXMemberCallExpr* cxx_member_call_expr,
                            FeatureInfo* feature_info_ptr,
                            Env* env_ptr,
                            std::vector<std::string>* fields_ptr) {
  if (cxx_member_call_expr == nullptr || feature_info_ptr == nullptr
      || env_ptr == nullptr || fields_ptr == nullptr) {
    return;
  }

  clang::Expr* caller = cxx_member_call_expr->getImplicitObjectArgument();
  if (clang::MemberExpr* callee = dyn_cast<clang::MemberExpr>(cxx_member_call_expr->getCallee())) {
    std::string callee_name = callee->getMemberDecl()->getNameAsString();
    if (callee_name == "push_back" && tool::is_int_vector(caller->getType())) {
      std::string caller_name = tool::trim_this(stmt_to_string(caller));
      if (feature_info_ptr->is_int_list_member(caller_name, caller->getType())) {
        if (cxx_member_call_expr->getNumArgs() == 1) {
          std::string arg_str = stmt_to_string(cxx_member_call_expr->getArg(0));
          if (is_integer(arg_str)) {
            LOG(INFO) << "add_int_list_member_single_value, caller_name: " << caller_name
                      << ", v: " << arg_str;
            feature_info_ptr->add_int_list_member_single_value(caller_name, std::stoi(arg_str));
          } else {
            if (clang::Expr* inner_expr = tool::get_inner_expr(cxx_member_call_expr->getArg(0))) {
              if (tool::is_common_info_enum(inner_expr->getType())) {
                if (absl::optional<int> enum_value = find_common_attr_int_value(inner_expr)) {
                  LOG(INFO) << "add_int_list_member_single_value, caller_name: "
                            << caller_name << ", v: " << *enum_value;
                  feature_info_ptr->add_int_list_member_single_value(caller_name, *enum_value);
                  // 不需要了
                  // rewriter_.RemoveText(cxx_member_call_expr);
                }
              }
            }
          }
        }
      }
    }
  }

  // bs extractor 所用字段。
  //
  // 主要分为普通字段和中间节点字段。
  // 普通字段是通过 attr_metas_.emplace_back 的字段，第一个函数参数即为具体的字段。
  // 中间节点字段指 PhotoInfo、LiveInfo 等嵌套的字段，需要根据名字获取具体的字段。
  // 如 BSGetPhotoInfoId, 即获取的是 photo_info.id 字段。
  std::string expr_str = stmt_to_string(cxx_member_call_expr);

  if (absl::StartsWith(expr_str, "this->")) {
    expr_str = expr_str.substr(6);
  }

  if (absl::StartsWith(expr_str, "bs_util.")) {
    expr_str = expr_str.substr(8);
  }

  if (absl::StartsWith(expr_str, "attr_metas_.emplace_back")) {
    if (cxx_member_call_expr->getNumArgs() == 1) {
      fields_ptr->emplace_back(stmt_to_string(cxx_member_call_expr->getArg(0)));
    } else {
      LOG(INFO) << "wrong number of args, should be 1, but is: " << cxx_member_call_expr->getNumArgs();
    }
  } else if (expr_str.find("fill_attr_metas") != std::string::npos) {
    LOG(INFO) << "dump bs functor";
    cxx_member_call_expr->dump();
    if (clang::Expr* caller = cxx_member_call_expr->getImplicitObjectArgument()) {
      LOG(INFO) << "caller";
      caller->dump();
    }

    std::string middle_node;

    std::vector<std::string> arr = absl::StrSplit(expr_str, ".");
    if (arr.size() > 0 && arr[0].size() > 5) {
      middle_node = arr[0].substr(5);
      if (middle_node.size() > 0) {
        fields_ptr->emplace_back(middle_node);
      }
    }
  }
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
