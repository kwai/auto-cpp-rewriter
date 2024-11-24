#pragma once

#include <glog/logging.h>
#include <absl/types/optional.h>

#include <regex>
#include <sstream>
#include <memory>
#include <string>
#include <vector>

#include "clang/AST/Expr.h"

#include "RuleBase.h"
#include "../Env.h"
#include "../Tool.h"
#include "../Type.h"
#include "../handler/StrictRewriter.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class Env;

class GeneralRule: public RuleBase {
 public:
  using RuleBase::process;
  explicit GeneralRule(clang::Rewriter& rewriter): RuleBase(rewriter, "GeneralRule") {}  // NOLINT

  void process(clang::IfStmt* if_stmt, Env* env_ptr) override;

  void process(clang::ForStmt* for_stmt, Env* env_ptr) override;

  void process(clang::CXXForRangeStmt* cxx_for_range_stmt, Env* env_ptr) override;

  void process(clang::DeclStmt* decl_stmt, Env* env_ptr) override;

  void process(clang::CallExpr* call_expr, Env* env_ptr) override;

  void process(clang::CXXMemberCallExpr* cxx_member_call_expr, Env* env_ptr) override;

  void process(clang::MemberExpr* member_expr, Env* env_ptr) override;

  void process(clang::DeclRefExpr* decl_ref_expr, Env* env_ptr) override;

  void process(clang::CXXNullPtrLiteralExpr* cxx_null_ptr_literal_expr, Env* env_ptr) override;

  void process(clang::UnaryOperator* unary_operator, Env* env_ptr) override;

  void process(clang::BinaryOperator* binary_operator, Env* env_ptr) override;

  void process(clang::IntegerLiteral* integer_literal, Env* env_ptr) override;

  void process(clang::CXXOperatorCallExpr* cxx_operator_call_expr, Env* env_ptr) override;

  void process(clang::ReturnStmt* return_stmt, Env* env_ptr) override;

  void process(clang::CXXFunctionalCastExpr* cxx_functional_cast_expr, Env* env_ptr) override;

  void process(clang::GNUNullExpr* gnu_null_expr, Env* env_ptr) override;

  template<typename T>
  void process_loop(T* loop_stmt,
                    Env* env_ptr);

  void process_loop_range(clang::ForStmt* for_stmt,
                          Env* env_ptr,
                          const std::string& body_text);

  void process_loop_range(clang::CXXForRangeStmt* cxx_for_range_stmt,
                          Env* env_ptr,
                          const std::string& body_text);

  void replace_decl_ref_var(ExprInfo* expr_info_ptr, Env* env_ptr);
  std::string replace_get_norm_query(clang::CXXMemberCallExpr* cxx_member_call_expr);
  std::string rm_decl_type(clang::DeclStmt* decl_stmt);
};

template<typename T>
void GeneralRule::process_loop(T* loop_stmt,
                               Env* env_ptr) {
  auto& loop_info = env_ptr->mutable_loop_info();
  if (!loop_info) {
    LOG(INFO) << "cannot get loop_info from env! loop_stmt: " << stmt_to_string(loop_stmt);
    return;
  }

  clang::Stmt* body = loop_stmt->getBody();

  if (env_ptr->is_common_info_loop()) {
    if (env_ptr->get_method_name() == "Extract" || env_ptr->feature_name() == "ItemFilter") {
      if (const auto &loop_info = env_ptr->cur_loop_info()) {
        // cxx_for_range_stmt
        if (!loop_info->is_for_stmt() || loop_info->is_common_info_map()) {
          rewriter_.ReplaceText(loop_stmt, get_loop_body(body, false));
          return;
        }
      }

      return;
    } else {
      return;
    }
  }

  // reco user info
  if (loop_info->is_reco_user_info_loop()) {
    std::string s = rewriter_.getRewrittenText(loop_stmt);
    const auto &leaf_fields = loop_info->leaf_fields();
    LOG(INFO) << "reco_user_info_loop, leaf_field: " << absl::StrJoin(leaf_fields, ", ");
    if (leaf_fields.size() > 0) {
      std::string origin_size_var = loop_info->origin_size_var();
      if (origin_size_var.size() > 0) {
        std::regex p(origin_size_var);
        s = std::regex_replace(s, p, leaf_fields[0] + ".size()");

        rewriter_.ReplaceText(loop_stmt, s);
      } else {
        LOG(ERROR) << "origin_size_var is empty! loop_stmt: "
                   << stmt_to_string(loop_stmt);
        return;
      }
    } else {
      LOG(ERROR) << "leaf_fields is empty! loop_stmt: "
                 << stmt_to_string(loop_stmt);
    }

    return;
  }

  // 普通 proto map
  if (loop_info->is_general_proto_map_loop()) {
    const std::string& prefix_adlog = loop_info->prefix_adlog();
    if (prefix_adlog.size() > 0) {
      std::string prefix = tool::adlog_to_bs_enum_str(prefix_adlog);
      if (const auto& var_def = env_ptr->find_new_def(prefix)) {
        if (var_def->new_var_type() == NewVarType::MAP) {
          std::ostringstream oss_res;
          oss_res << "for (size_t idx = 0; idx < " << var_def->name() << ".size(); idx++) {\n    "
                  << get_compound_stmt_rewritten_text(body)
                  << "\n}\n    ";

          std::string bs_text = oss_res.str();
          if (bs_text.size() > 0) {
            rewriter_.ReplaceText(loop_stmt, bs_text);
          }
        }
      } else {
        LOG(INFO) << "cannot find map def in env, bs_enum_str: " << prefix;
      }
    }
  }

  // common info 循环，if 条件需要在知道 common info 变量类型后才知道, 因此只能在 loop 替换时候处理
  if (const auto& common_info_normal = env_ptr->cur_common_info_normal()) {
    env_ptr->update_template_common_info_values();
    LOG(INFO) << "update template common info values, detail size: "
              << common_info_normal->common_info_details_size();
    std::ostringstream oss_res;
    const std::vector<std::shared_ptr<CommonInfoLeaf>>& common_info_details =
      common_info_normal->common_info_details();

    if (common_info_normal->is_check_equal()) {
      for (size_t i = 0; i < common_info_details.size(); i++) {
        LOG(INFO) << "i: " << i;
        oss_res << common_info_normal->get_bs_rewritten(&rewriter_, i)
                << "\n\n";
      }
    } else {
      oss_res << common_info_normal->get_bs_wrap_text(get_loop_body_without_if(body));
    }

    std::string bs_text = oss_res.str();
    if (bs_text.size() > 0) {
      rewriter_.ReplaceText(loop_stmt, bs_text);
    }

    // 可能有多个 common_info_normal, 每一个处理完必须清除 env 中的信息。
    if (auto& mutable_common_info_normal = env_ptr->cur_mutable_common_info_normal()) {
      mutable_common_info_normal.reset();
    }
  }

  // comon info enum 通过模板参数传进来
  if (const auto& common_info_fixed_list = env_ptr->cur_common_info_fixed_list()) {
    std::ostringstream oss_res;
    const std::vector<std::shared_ptr<CommonInfoFixed>> &common_info_details =
        common_info_fixed_list->common_info_details();
    for (size_t i = 0; i < common_info_details.size(); i++) {
      oss_res << common_info_fixed_list->get_bs_rewritten(&rewriter_, i)
              << "\n\n";
    }

    std::string bs_text = oss_res.str();
    if (bs_text.size() > 0) {
      rewriter_.ReplaceText(loop_stmt, bs_text);
    }

    if (auto& mutable_common_info_fixed_list = env_ptr->cur_mutable_common_info_fixed_list()) {
      mutable_common_info_fixed_list.reset();
    }
  }

  if (const auto& common_info_multi_map = env_ptr->cur_common_info_multi_map()) {
    std::ostringstream oss_range;
    std::string iter_str = loop_info->loop_iter();
    std::string repeated_field = "BSRepeatedField<int64_t>";
    if (env_ptr->is_combine_feature() && !tool::is_item_field(common_info_multi_map->prefix())) {
      repeated_field = "BSRepeatedField<int64_t, true>";
    }

    // get body_text
    std::ostringstream oss_body;
    for (auto it_body = body->child_begin(); it_body != body->child_end(); it_body++) {
      if (clang::BreakStmt* break_stmt = dyn_cast<clang::BreakStmt>(*it_body)) {
        continue;
      }

      std::string new_text = rewriter_.getRewrittenText(*it_body);
      oss_body << fix_semicolon(new_text) << "\n";
    }

    oss_range << "for (auto " << iter_str << " = " << common_info_multi_map->map_name() << ".begin(); "
              << iter_str << " != " << common_info_multi_map->map_name() << ".end(); "
              << iter_str << "++) {\n"
              << repeated_field << " " << common_info_multi_map->attr_name()
              << "(*bs, iter->first, pos);\n"
              << oss_body.str()
              << " }\n";
    rewriter_.ReplaceText(loop_stmt, oss_range.str());
  }

  // seq list 不替换
  if (const auto &loop_info = env_ptr->get_loop_info()) {
    if (!loop_info->is_for_stmt() && loop_info->is_seq_list_loop()) {
      rewriter_.ReplaceText(loop_stmt, loop_info->origin_stmt_str());
    }
  }

  std::ostringstream oss_new_defs;
  oss_new_defs << env_ptr->get_all_new_defs() << "\n";

  rewriter_.InsertTextBefore(loop_stmt, oss_new_defs.str());
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
