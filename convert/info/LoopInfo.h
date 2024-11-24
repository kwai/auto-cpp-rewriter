#pragma once

#include <absl/types/optional.h>

#include <map>
#include <string>
#include <vector>

#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/AST/AST.h"
#include "clang/AST/StmtCXX.h"
#include "clang/AST/ASTConsumer.h"

#include "../Tool.h"
#include "../Type.h"
#include "InfoBase.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

enum class LoopStage {
  INIT,
  BODY,
  END
};

/// 保存 loop 相关的信息
class LoopInfo: public InfoBase {
 public:
  LoopInfo() = default;
  explicit LoopInfo(clang::ForStmt* for_stmt):
    is_for_stmt_(true),
    for_stmt_(for_stmt),
    loop_stage_(LoopStage::INIT) {}
  explicit LoopInfo(clang::CXXForRangeStmt* cxx_for_range_stmt):
    is_for_stmt_(false),
    cxx_for_range_stmt_(cxx_for_range_stmt),
    loop_stage_(LoopStage::INIT) {}

  bool is_for_stmt() const { return is_for_stmt_; }

  clang::ForStmt* for_stmt() { return for_stmt_; }
  clang::CXXForRangeStmt* cxx_for_range_stmt() { return cxx_for_range_stmt_; }

  void set_loop_state(LoopStage loop_stage) { loop_stage_ = loop_stage; }
  LoopStage loop_stage() const { return loop_stage_; }
  LoopStage loop_stage() { return loop_stage_; }

  void set_loop_iter(const std::string& loop_iter) { loop_iter_ = loop_iter; }
  const std::string& loop_iter() const { return loop_iter_; }

  clang::Expr* loop_var_expr() const { return loop_var_expr_; }
  void set_loop_var_expr(clang::Expr* loop_var_expr) { loop_var_expr_ = loop_var_expr; }
  std::string loop_var_expr_str() const;

  const std::string& loop_var() const { return loop_var_; }
  void set_loop_var(const std::string& loop_var) { loop_var_ = loop_var; }

  const std::string &prefix_adlog() const { return prefix_adlog_; }
  void set_prefix_adlog(const std::string &prefix_adlog) { prefix_adlog_ = prefix_adlog; }

  bool is_common_info_list_map() const { return is_common_info_list_map_; }
  void set_is_common_info_list_map(bool v) { is_common_info_list_map_ = v; }

  bool is_common_info_map() const {return is_common_info_map_;}
  void set_is_common_info_map(bool v) {is_common_info_map_ = v;}

  bool is_repeated_common_info() const { return is_repeated_common_info_; }
  void set_is_repeated_common_info(bool v) { is_repeated_common_info_ = v; }

  bool is_proto_list_loop() const { return is_proto_list_loop_; }
  void set_is_proto_list_loop(bool v) { is_proto_list_loop_ = v; }

  bool is_general_proto_map_loop() const { return is_general_proto_map_loop_; }
  void set_is_general_proto_map_loop(bool v) { is_general_proto_map_loop_ = v; }

  bool is_child_proto_list_loop() const { return is_child_proto_list_loop_; }
  void set_is_child_proto_list_loop(bool v) { is_child_proto_list_loop_ = v; }

  bool is_int_list_member_loop() const { return is_int_list_member_loop_; }
  void set_is_int_list_member_loop(bool v) { is_int_list_member_loop_ = v; }

  const std::vector<int>& int_list_member_values() const { return int_list_member_values_; }
  void set_int_list_member_values(const std::vector<int>& values) { int_list_member_values_ = values; }

  const absl::optional<size_t>& int_list_index() const { return int_list_index_; }
  void set_int_list_index(size_t x) { int_list_index_.emplace(x); }

  std::string origin_stmt_str() const;

  bool is_seq_list_loop() const { return is_seq_list_loop_; }
  void set_is_seq_list_loop(bool v) { is_seq_list_loop_ = v; }

  const std::string& loop_var_expr_bs_enum_str() const { return loop_var_expr_bs_enum_str_; }
  void set_loop_var_expr_bs_enum_str(const std::string& s) { loop_var_expr_bs_enum_str_ = s; }

  bool is_middle_node_proto_list_loop() const { return is_middle_node_proto_list_loop_; }
  void set_is_middle_node_proto_list_loop(bool v) { is_middle_node_proto_list_loop_ = v; }

  bool is_double_list_loop() const;
  bool is_double_list_inner_loop() const;
  bool is_double_list_outer_loop() const;

  bool is_query_token_loop() const {return is_query_token_loop_;}
  void set_is_query_token_loop(bool v) {is_query_token_loop_ = v;}

  const std::string& loop_var_type() const { return (loop_var_type_); }
  void set_loop_var_type(const std::string& loop_var_type) { loop_var_type_ = loop_var_type; }

  const std::string& origin_size_var() const { return (origin_size_var_); }
  void set_origin_size_var(const std::string& origin_size_var) { origin_size_var_ = origin_size_var; }

  void add_leaf_field(const std::string& field) { leaf_fields_.emplace_back(field); }
  const std::vector<std::string>& leaf_fields() const { return leaf_fields_; }

  bool is_reco_user_info_loop() const { return is_reco_user_info_loop_; }
  void set_is_reco_user_info_loop(bool v) { is_reco_user_info_loop_ = v; }

 private:
  bool is_for_stmt_;
  clang::ForStmt* for_stmt_;
  clang::CXXForRangeStmt* cxx_for_range_stmt_;
  LoopStage loop_stage_;
  std::string loop_iter_;
  std::string loop_var_;
  std::string loop_var_type_;
  clang::Expr* loop_var_expr_ = nullptr;
  std::string loop_var_expr_bs_enum_str_;
  std::string prefix_adlog_;

  std::string origin_size_var_;
  std::vector<std::string> leaf_fields_;

  std::vector<int> int_list_member_values_;
  absl::optional<size_t> int_list_index_;

  bool is_common_info_map_ = false;
  bool is_common_info_list_map_ = false;
  bool is_repeated_common_info_ = false;
  bool is_proto_list_loop_ = false;
  bool is_general_proto_map_loop_ = false;
  bool is_child_proto_list_loop_ = false;
  bool is_int_list_member_loop_ = false;
  bool is_seq_list_loop_ = false;
  bool is_middle_node_proto_list_loop_ = false;
  bool is_query_token_loop_ = false;
  bool is_reco_user_info_loop_ = false;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
