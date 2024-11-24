#include <absl/types/optional.h>
#include <sstream>
#include <vector>
#include <string>
#include <glog/logging.h>

#include "clang/AST/Decl.h"

#include "../Env.h"
#include "../Tool.h"
#include "../ExprInfo.h"
#include "../ExprParser.h"
#include "../info/IfInfo.h"
#include "../handler/StrictRewriter.h"
#include "SeqListRule.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

void SeqListRule::process(clang::IfStmt* if_stmt,
                          Env* env_ptr) {
  if (env_ptr == nullptr) {
    return;
  }

  if (const auto& if_info = env_ptr->cur_if_info()) {
    if (if_info->is_check_seq_list_cond()) {
      if (const auto& seq_list_info = env_ptr->get_seq_list_info()) {
        std::ostringstream oss;
        if (seq_list_info->var_name().size() > 0) {
          oss << "if (!" << seq_list_info->var_name() << ".is_empty()) {\n    ";
        } else {
          oss << "if (!" << seq_list_info->root_name() << "->is_empty()) {\n    ";
        }
        oss << get_compound_stmt_rewritten_text(if_stmt->getThen());
        oss << "\n}\n   ";

        rewriter_.ReplaceText(if_stmt, oss.str());
      }
    }
  }
}

void SeqListRule::process(clang::DeclStmt *decl_stmt, Env *env_ptr) {
  if (clang::VarDecl* var_decl = dyn_cast<clang::VarDecl>(decl_stmt->getSingleDecl())) {
    clang::QualType qual_type = var_decl->getType();
    if (tool::is_repeated_proto_ptr(qual_type) && !tool::is_reco_proto(qual_type)) {
      if (absl::optional<std::string> inner_type = tool::get_repeated_proto_inner_type(qual_type)) {
        // 简单处理，目前的几个类都是 user
        bool is_combine_user = env_ptr->is_combine_feature();
        std::string bs_repeated_type = tool::get_bs_repeated_field_type(*inner_type, is_combine_user);
        std::ostringstream oss;
        oss << bs_repeated_type
            << var_decl->getNameAsString()
            << ";\n";
        rewriter_.ReplaceText(decl_stmt, oss.str());
      } else {
        LOG(INFO) << "cannot find inner_type for repeated proto, decl_stmt: " << stmt_to_string(decl_stmt);
      }
    }
  }
}

void SeqListRule::process(clang::UnaryOperator* unary_operator, Env *env_ptr) {
  auto expr_info_ptr = parse_expr(unary_operator, env_ptr);
  if (expr_info_ptr == nullptr) {
    return;
  }

  // const auto& seq_list = *seq_list_ptr;
  if (expr_info_ptr->is_seq_list_root_deref()) {
    if (expr_info_ptr->call_expr_params_size() == 1) {
      ExprInfo* param = expr_info_ptr->call_expr_param(0);
      if (param != nullptr) {
        if (param->is_seq_list_root_ref()) {
          rewriter_.ReplaceText(unary_operator, param->origin_expr_str() + ";\n");
        }
      }
    }
  }
}

void SeqListRule::process(clang::BinaryOperator* binary_operator, Env *env_ptr) {
  if (env_ptr == nullptr) {
    return;
  }

  if (binary_operator->isAssignmentOp()) {
    // 替换取地址为最后的 BSRepeatedField 对应的变量。
    // ad/ad_algorithm/feature/fast/impl/extract_mmu_top28_seq_sparse.h
    // const auto &common_infos = adlog.user_info().common_info_attr();
    // for (const auto &userAttr : common_infos) {
    //   if (userAttr.name_value() == to_use_common_info_name_value) {
    //     seq_list = &userAttr.int_list_value();
    //     break;
    //   }
    // }
    clang::Expr* init_expr = binary_operator->getRHS();
    auto expr_info_ptr = parse_expr(init_expr, env_ptr);
    if (expr_info_ptr->is_address_expr() &&
        expr_info_ptr->call_expr_params_size() == 1) {
      ExprInfo *param_info_ptr = expr_info_ptr->call_expr_param(0);
      if (param_info_ptr != nullptr &&
          param_info_ptr->is_common_info_list_method()) {
        if (const auto &common_info_fixed_list = env_ptr->get_common_info_fixed_list()) {
          if (const auto last_detail = common_info_fixed_list->last_common_info_detail()) {
            std::string bs_enum_str = last_detail->get_bs_enum_str();
            if (const auto &bs_var = env_ptr->find_new_def(bs_enum_str)) {
              rewriter_.ReplaceText(init_expr, std::string("std::move(") + bs_var->name() + ")");
            }
          }
        } else if (const auto& common_info_normal = env_ptr->get_common_info_normal()) {
          if (const auto& common_info_detail = common_info_normal->last_common_info_detail()) {
            std::string bs_enum_str = common_info_detail->get_bs_enum_str();
            if (const auto& bs_var = env_ptr->find_new_def(bs_enum_str)) {
              rewriter_.ReplaceText(init_expr, std::string("std::move(") + bs_var->name() + ")");
            }
          }
        }
      }
    }
  }

  // timestamp_seq_list == nullptr
  // timestamp_list != nullptr
  if (binary_operator->isComparisonOp()) {
    clang::Expr* left_expr = binary_operator->getLHS();
    clang::Expr* right_expr = binary_operator->getRHS();
    auto expr_info_ptr = parse_expr(left_expr, env_ptr);
    if (expr_info_ptr != nullptr &&
        expr_info_ptr->is_decl_ref_expr() &&
        expr_info_ptr->origin_expr() != nullptr &&
        tool::is_repeated_proto_ptr(expr_info_ptr->origin_expr()->getType()) &&
        !expr_info_ptr->is_reco_proto_type()) {
      std::string op = binary_operator->getOpcodeStr().str();
      if (stmt_to_string(right_expr) == "nullptr") {
        if (op == "==") {
          rewriter_.ReplaceText(binary_operator, expr_info_ptr->origin_expr_str() + ".is_empty()");
        } else if (op == "!=") {
          std::string s = std::string("!") + expr_info_ptr->origin_expr_str() + ".is_empty()";
          rewriter_.ReplaceText(binary_operator, s);
        }
      }
    }
  }
}

void SeqListRule::process(clang::CallExpr *call_expr, Env *env_ptr) {
  std::shared_ptr<ExprInfo> expr_info_ptr = parse_expr(call_expr, env_ptr);
  if (expr_info_ptr == nullptr) {
    LOG(INFO) << "expr_info_ptr is nullptr!";
  }

  if (expr_info_ptr->is_seq_list_reco_proto_type() || expr_info_ptr->is_seq_list_root()) {
    std::string s = tool::trim_this(expr_info_ptr->to_string());
    rewriter_.ReplaceText(call_expr, tool::replace_adlog_to_bslog(s));
  }
}

void SeqListRule::process(clang::CXXMemberCallExpr* cxx_member_call_expr, Env* env_ptr) {
  // item_id_seq_list->size()  => item_id_seq_list.size()
  auto expr_info_ptr = parse_expr(cxx_member_call_expr, env_ptr);
  if (expr_info_ptr == nullptr) {
    return;
  }

  if (expr_info_ptr->callee_name() == "size") {
    if (auto parent = expr_info_ptr->parent()) {
      if (parent->is_decl_ref_expr() && parent->origin_expr() != nullptr) {
        if (tool::is_repeated_proto_ptr(parent->origin_expr()->getType())) {
          std::string s = parent->origin_expr_str() + ".size()";
          rewriter_.ReplaceText(cxx_member_call_expr, s);
        }
      }
    }
  }
}

void SeqListRule::process(clang::ReturnStmt* return_stmt, Env* env_ptr) {
  // 简单处理, nullptr 直接替换为 BSRepatedField<int64_t>{}
  if (env_ptr->get_method_name() == "GetSeqList") {
    clang::Expr *ret_value = return_stmt->getRetValue();
    if (stmt_to_string(ret_value) == "nullptr") {
      rewriter_.ReplaceText(ret_value, "BSRepeatedField<int64_t>{}");
    }
  }
}

void SeqListRule::process(clang::CXXDependentScopeMemberExpr *cxx_dependent_scope_member_expr, Env *env_ptr) {
  auto expr_info_ptr = parse_expr(cxx_dependent_scope_member_expr, env_ptr);
}

void SeqListRule::process(clang::ArraySubscriptExpr *array_subscript_expr, Env *env_ptr) {
  auto expr_info_ptr = parse_expr(array_subscript_expr, env_ptr);
}

void SeqListRule::process(clang::CXXForRangeStmt* cxx_for_range_stmt, Env *env_ptr) {
}

} // convert
}  // namespace ad_algorithm
}  // namespace ks
