#pragma once

#include <absl/types/optional.h>

#include <map>
#include <string>
#include <set>

#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/AST/AST.h"
#include "clang/AST/StmtCXX.h"
#include "clang/AST/ASTConsumer.h"

#include "../Type.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

enum class IfStage {
  COND,
  THEN,
  ELSE,
  END
};

/// 保存 if 相关的信息
class IfInfo {
 public:
  IfInfo() = default;
  explicit IfInfo(clang::IfStmt* if_stmt): if_stmt_(if_stmt), if_stage_(IfStage::COND) {}

  clang::IfStmt* if_stmt() const { return if_stmt_; }

  void set_if_stage(IfStage if_stage) { if_stage_ = if_stage; }
  IfStage if_stage() const { return if_stage_; }

  bool is_check_item_pos_cond() const;

  bool is_check_middle_node_root_cond() const { return is_check_middle_node_root_; }
  void set_is_check_middle_node_root_cond(bool v) { is_check_middle_node_root_ = v; }

  bool is_check_common_info_normal_cond() const { return is_check_common_info_normal_; }
  void set_is_check_common_info_normal_cond(bool v) { is_check_common_info_normal_ = v; }

  bool is_check_common_info_multi_cond() const { return is_check_common_info_multi_; }
  void set_is_check_common_info_multi_cond(bool v) { is_check_common_info_multi_ = v; }

  bool is_check_common_info_fixed_cond() const { return is_check_common_info_fixed_; }
  void set_is_check_common_info_fixed_cond(bool v) { is_check_common_info_fixed_ = v; }

  bool is_check_common_info_cond() const {
    return is_check_common_info_ ||
      is_check_common_info_normal_ ||
      is_check_common_info_multi_ ||
      is_check_common_info_fixed_;
  }
  void set_is_check_common_info_cond(bool v) { is_check_common_info_ = v; }

  bool is_check_action_detail_cond() const { return is_check_action_detail_; }
  void set_is_check_action_detail_cond(bool v) { is_check_action_detail_ = v; }

  void add_cond_var_type(ExprType cond_var_type) { cond_var_types_.insert(cond_var_type); }
  bool has_cond_var_type(ExprType cond_var_type) const {
    return cond_var_types_.find(cond_var_type) != cond_var_types_.end();
  }

  void set_common_info_index(size_t index) { common_info_index_.emplace(index); }
  const absl::optional<size_t>& common_info_index() const { return (common_info_index_); }

  void set_common_info_value(int value) { common_info_value_.emplace(value); }
  const absl::optional<int>& common_info_value() const { return (common_info_value_); }

  void set_common_info_int_name(const std::string& int_name) {common_info_int_name_.emplace(int_name);}
  const absl::optional<std::string>& common_info_int_name() const {return (common_info_int_name_);}

  void update_check_equal(const std::string& op);
  bool is_check_equal() const { return is_check_equal_; }
  void set_is_check_equal(bool v) { is_check_equal_ = v; }
  bool is_check_not_equal() const { return is_check_not_equal_; }

  bool is_check_item_pos_include() const { return is_check_item_pos_include_; }
  void set_is_check_item_pos_include_cond(bool v) { is_check_item_pos_include_ = v; }

  bool is_check_seq_list_cond() const { return is_check_seq_list_; }
  void set_is_check_seq_list_cond(bool v) { is_check_seq_list_ = v; }

  bool is_check_common_info_map_end() const { return is_check_common_info_map_end_; }
  void set_is_check_common_info_map_end(bool v) { is_check_common_info_map_end_ = v; }

  bool is_check_common_info_list_size_not_equal() const {return is_check_common_info_list_size_not_equal_;}
  void set_is_check_common_info_list_size_not_equal(bool v) {is_check_common_info_list_size_not_equal_ = v;}

  const absl::optional<std::string>& left_expr_str() const { return (left_expr_str_); }
  void set_left_expr_str(const std::string& left_expr_str) { left_expr_str_.emplace(left_expr_str); }

  bool is_body_only_break() const;

 private:
  clang::IfStmt* if_stmt_ = nullptr;
  IfStage if_stage_;
  std::set<ExprType> cond_var_types_;
  bool is_check_middle_node_root_ = false;
  bool is_check_common_info_multi_ = false;
  bool is_check_common_info_normal_ = false;
  bool is_check_common_info_fixed_ = false;
  bool is_check_common_info_ = false;
  bool is_check_common_info_map_end_ = false;
  bool is_check_common_info_list_size_not_equal_ = false;
  bool is_check_action_detail_ = false;
  bool is_check_equal_ = false;
  bool is_check_not_equal_ = false;
  bool is_check_item_pos_include_ = false;
  bool is_check_seq_list_ = false;
  absl::optional<size_t> common_info_index_;
  absl::optional<int> common_info_value_;
  absl::optional<std::string> common_info_int_name_;
  absl::optional<std::string> left_expr_str_;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
