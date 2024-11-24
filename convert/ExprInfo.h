#pragma once

#include <absl/types/optional.h>
#include <glog/logging.h>

#include <list>
#include <utility>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "clang/AST/AST.h"
#include "clang/AST/Expr.h"
#include "clang/AST/Type.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class Env;
class StrictRewriter;
enum class CommonInfoValueType;
enum class NewVarType;

/// 表达式 `clang::Expr` 的各种信息。
class ExprInfo {
 public:
  ExprInfo() = default;
  explicit ExprInfo(clang::Expr* expr, Env* env_ptr): expr_(expr), env_ptr_(env_ptr) {}

  const std::string& callee_name() const { return callee_name_; }
  const std::vector<clang::Expr*>& params() const { return params_; }

  /// 以下几种情况需要替换:
  /// 1. 来自 adlog 的字段, 如普通 adlog 字段, 中间节点字段, common info 字段。
  /// 2. 自定义变量，但是包含 CommonAttrInfo, 如 std::vector<CommonAttrInfo>
  bool need_replace() const;

  /// get bs field value from adlog expr, like adlog.item.id, kv.first for map, or attrs[i] for list
  std::string get_bs_field_value() const;
  std::string get_bs_field_value_normal() const;
  std::string get_bs_field_value_loop_var_size() const;
  std::string get_bs_field_value_middle_node() const;
  std::string get_bs_field_value_action_detail_leaf(const std::string& param) const;
  std::string get_bs_field_value_general_proto_list_size_method() const;
  std::string get_bs_field_value_query_token() const;
  std::string get_bs_field_value_reco_user_info() const;
  std::string get_bs_field_value_middle_node_leaf_list_size_method() const;

  clang::Expr* find_action_detail_index_param() const;

  /// list or map definition
  std::string get_bs_scalar_def() const;
  std::string get_bs_scalar_def(const std::string& name) const;

  /// 返回单值判断条件是否存在的 bs 表达式。
  /// 返回 xxx_exists, xxx_exists 的定义需要提前放到 env_ptr 中
  std::string get_bs_scalar_exists_def() const;
  std::string get_bs_scalar_exists_def(const std::string& name) const;
  std::string get_bs_scalar_def_helper(bool is_exists_expr, const std::string& name) const;

  std::string get_bs_list_def() const;
  std::pair<std::string, std::string> get_map_kv_type() const;
  std::string get_bs_map_def() const;

  bool is_ignore_callee_name() const;
  bool is_keep_callee_name() const;
  std::string get_bs_enum_str() const;
  std::string get_bs_exists_enum_str() const;
  std::string get_adlog_expr() const;
  std::string get_adlog_field_str() const;

  clang::Expr* expr() const { return expr_; }
  clang::DeclRefExpr* origin_expr() const { return origin_expr_; }
  void set_origin_expr(clang::DeclRefExpr* origin_expr) { origin_expr_ = origin_expr; }
  std::string origin_expr_str() const;

  void set_parent(std::shared_ptr<ExprInfo> parent) { parent_ = parent; }
  void set_env_ptr(Env* env_ptr) { env_ptr_ = env_ptr; }
  void set_expr(clang::Expr* expr) { expr_ = expr; }
  void set_callee_name(const std::string& callee_name) { callee_name_ = callee_name; }
  void add_param(clang::Expr* param) { params_.push_back(param); }

  bool is_from_normal_adlog() const;

  /// 来自 item，用于 infer 的 ItemFilter
  bool is_from_adlog_item() const;
  bool is_from_middle_node() const;
  bool is_from_repeated_common_info() const;
  bool is_from_action_detail_map() const;
  bool is_from_adlog() const;

  bool is_middle_node_root() const;
  std::string get_bs_middle_node_leaf() const;
  std::string get_middle_node_root_name() const;
  std::string get_middle_node_field() const;

  std::string get_bs_middle_node_leaf_trim_size() const;

  /// clang::DeclRefExpr*
  bool is_decl_ref_expr() const;
  std::string get_first_decl_ref() const;

  bool is_iter_second() const;

  bool is_from_list() const;
  bool is_from_map() const;
  bool is_basic() const;
  bool is_nullptr() const;
  bool is_basic_scalar() const;

  absl::optional<std::string> get_builtin_type_str() const;

  bool is_common_attr_info_enum() const;
  bool is_action_detail_list_expr() const;

  bool is_action_detail_map() const;
  bool is_action_detail_find_expr() const;
  bool is_action_detail_map_end() const;
  bool is_action_info_type() const;
  bool is_action_detail_leaf() const;
  bool is_action_detail_map_size_method() const;

  /// 多个 common info
  bool is_find_name_value() const;
  std::string get_common_info_multi_map_name() const;
  std::string get_common_info_multi_attr_name() const;
  bool is_common_info_multi_map_end() const;
  bool is_common_info_multi_map_attr() const;
  bool is_common_info_scalar_method() const;
  bool is_common_info_list_method() const;
  bool is_common_info_map_method() const;
  bool is_common_info_map_find_expr() const;
  bool is_common_info_method() const;
  bool is_common_info_size_method() const;
  bool is_common_info_empty_method() const;
  bool is_common_info_list_size_method() const;
  bool is_common_info_list_size_method_divide_by_int() const;
  bool is_common_info_leaf_method() const;
  bool is_common_info_name_value() const;
  bool is_common_info_compare_int_value(Env* env_ptr) const;
  bool is_name_value_method() const;

  bool is_repeated_common_info() const;
  bool is_repeated_common_info_size() const;

  bool is_common_info_struct_type() const;
  bool is_common_info_map_end() const;
  bool is_common_info_map_iter() const;
  bool is_common_info_map_iter_second() const;

  absl::optional<CommonInfoValueType> get_common_info_value_type() const;

  /// ItemType::AD_DSP
  bool is_item_type_enum() const;

  /// using auto_cpp_rewriter::AdCallbackLog;
  bool is_ad_callback_log_enum() const;

  /// auto_cpp_rewriter::AD_ITEM_CLICK
  bool is_ad_action_type_enum() const;

  bool is_cxx_member_call_expr() const;
  bool is_call_expr() const;
  bool is_ad_enum() const;

  bool is_item_field() const;
  bool is_adlog_user_field() const;

  std::string get_ad_enum_name() const;

  /// adlog
  bool is_adlog_root() const;

  bool is_first_param_adlog_item() const;
  bool is_unary_expr() const;
  std::string get_first_caller_name() const;
  absl::optional<int> get_action() const;
  absl::optional<int> find_int_ref_value(const std::string& name) const;

  /// action detail prefix, 不包含后面的 key
  absl::optional<std::string> get_action_detail_prefix_adlog() const;

  /// 遍历 action_vec, 逻辑有点复杂。
  absl::optional<int> find_first_int_in_loop_expr(const std::string& arg) const;

  absl::optional<int> get_common_attr_int_value() const;
  absl::optional<std::string> get_common_info_prefix() const;
  absl::optional<std::string> get_common_info_prefix_adlog() const;

  inline void add_cur_expr_str(const std::string& s) { cur_expr_str_.push_back(s); }
  const std::vector<std::string>& cur_expr_str() const { return cur_expr_str_; }

  bool contains_loop_var() const;

  std::shared_ptr<ExprInfo> parent() const { return parent_; }

  bool has_decl_ref() const;

  /// caller 是自定义的变量, 不是 item, 如 product_name.find("xxx");
  bool is_caller_custom_decl_ref() const;

  /// caller 是自定义的变量, 并且是 str
  bool is_caller_str_ref() const;

  bool is_str_decl_ref() const;
  bool is_item_ref() const;
  bool is_string() const;

  bool is_integral() const;
  absl::optional<int> get_int_value() const;
  absl::optional<int> get_int_ref_value() const;
  bool is_int_ref() const;
  absl::optional<std::string> get_ref_var_name() const;

  bool is_template_int_ref() const;

  /// 来自 common info enum 属性的引用
  bool is_common_info_enum_member_ref() const;

  std::string to_string() const;
  bool is_get_norm_query() const;
  bool contains_template_parameter() const;
  absl::optional<std::string> get_template_action() const;
  absl::optional<std::string> get_action_detail_field_name() const;

  bool is_var_proto_list() const;
  bool is_var_proto_map() const;
  bool is_repeated_proto_message_type() const;

  bool is_parent_str_type() const;
  bool is_parent_str_ref() const;
  bool is_caller_loop_var() const;
  std::string callee_with_params(const StrictRewriter& rewriter) const;
  void add_call_expr_param(std::shared_ptr<ExprInfo> expr_info_ptr);
  ExprInfo* call_expr_param(size_t index) const;
  size_t call_expr_params_size() const { return call_expr_params_.size(); }

  bool is_cxx_operator_call_expr() const;
  bool is_cxx_operator_call_expr_deref() const;
  NewVarType get_new_var_type() const;
  bool has_common_attr_int_value_in_env() const;
  bool has_common_attr_int_value_in_expr() const;
  absl::optional<int> get_common_attr_int_value_in_expr() const;
  absl::optional<std::string> get_common_info_enum_name_in_expr() const;

  bool is_int_list_member_ref() const;
  bool is_int_list_var_ref() const;
  bool is_action_detail_list() const;
  bool is_action_detail_list_size_expr() const;
  std::string get_bs_field_value_action_detail_list_size() const;

  const std::string& raw_expr_str() const { return raw_expr_str_; }
  void set_raw_expr_str(const std::string& raw_expr_str) { raw_expr_str_ = raw_expr_str; }

  bool is_parent_this() const;
  bool is_pos_ref() const;

  bool is_has_reco_user_info_method() const;
  bool is_reco_user_info_method() const;

  /// 表示不需要替换的字段，不是来自 adlog proto，如 reco_user_info, ad_user_history_photo_embedding 等。
  /// 开始只处理了 reco_user_info，后面又添加了其他字段，之后再找时间换个名字。
  bool is_from_reco_user_info() const;

  /// 不考虑是否替换，判断实际是否是来自 reco_user_info。
  bool is_from_reco_user_info_real() const;

  bool is_action_info() const;
  bool is_parent_action_info() const;

  bool is_repeated_action_info() const;
  bool is_parent_repeated_action_info() const;

  clang::Expr* caller() const { return caller_; }
  void set_caller(clang::Expr* caller) { caller_ = caller; }

  ExprInfo* caller_info() const;
  void set_caller_info(std::shared_ptr<ExprInfo> caller_info);

  bool is_seq_list_root() const;
  bool is_seq_list_root_ref() const;
  bool is_seq_list_root_deref() const;
  bool is_seq_list_ptr() const;
  bool is_seq_list() const;
  bool is_from_seq_list() const;
  bool is_seq_list_reco_proto_type() const;

  bool is_from_seq_list_reco() const;

  bool is_address_expr() const;
  bool is_ptr_deref_expr() const;

  /// 普通的 adlog 变量
  bool is_general_adlog_var() const;
  bool is_reco_proto_type() const;
  bool is_repeated_proto_type() const;
  bool is_repeated_proto_iterator_type() const;
  bool is_repeated_proto_ptr() const;
  bool is_map_repeated_int_list_type() const;
  bool is_from_int_list_member() const;
  bool is_map_int_int_type() const;
  bool is_str_type() const;
  bool is_var_proto_message_type() const;

  bool is_member_expr() const;
  absl::optional<std::string> find_int_list_member_name() const;

  bool is_loop_iter_end() const;

  absl::optional<std::string> get_adlog_field_str_after_loop_var() const;
  bool is_implicit_loop_var() const;
  bool is_from_implicit_loop_var() const;
  bool is_deref_implicit_loop_begin_expr() const;

  bool is_loop_var_ref() const;
  bool is_loop_var_size_method() const;
  bool is_parent_loop_var_ref() const;
  bool is_binary_op_expr() const;

  absl::optional<std::string> find_int_param() const;

  bool is_list_size_method() const;

  /// 特指叶子节点, 不包括 common info 数据对应的节点
  bool is_general_proto_list_size_method() const;

  /// 来自中间节点的 list size
  bool is_middle_node_leaf_list_size_method() const;
  std::string get_bs_enum_str_trim_size() const;
  bool is_str_concat() const;

  bool is_cxx_functional_cast_expr() const;
  bool is_enum_proto_call() const;

  /// QueryToken
  bool is_query_token_call() const;
  bool is_from_query_token() const;

  /// PhotoText
  bool is_photo_text_call() const;
  bool is_from_photo_text() const;

  bool is_parent_from_proto_map_string_float() const;

  bool is_proto_map_string_float_ptr_type() const;
  bool is_proto_map_string_float_ref() const;
  bool is_proto_map_string_float_iter_type() const;
  bool is_proto_map_string_float_iter_first() const;
  bool is_proto_map_string_float_iter_second() const;
  bool is_proto_map_string_float_size() const;
  bool is_proto_map_string_float_end() const;
  bool is_photo_text_find_expr() const;

  clang::Expr* get_proto_map_string_float_loop_var_expr() const;

  clang::Expr* get_loop_var_expr() const;
  bool is_method_is_train() const;
  bool is_parent_common_info_map_method() const;

  /// proto2 比较特殊，基础类型也有对应的 has_xxx 方法。
  bool is_proto2_has_method(const std::string& method_name) const;
  bool is_char_arr_type() const;
  bool is_decl_init_expr() const;
  bool is_in_decl_stmt() const;
  bool is_repeated_proto_list_leaf_type() const;

  /// 是否是 bs 字段。
  bool is_bslog_field_enum_decl() const;
  bool is_bslog_field_var_decl() const;

  /// 枚举值或者变量 ref。
  bool is_bslog_field_enum_ref() const;

 protected:
  Env* env_ptr_ = nullptr;

  int level_ = 0;

  /// 保留完整 expr 方便之后的处理
  clang::Expr* expr_ = nullptr;

  /// DeclRefExpr
  clang::DeclRefExpr* origin_expr_ = nullptr;
  std::string adlog_expr_;

  /// enum key
  std::string bs_expr_;
  std::string feature_type_;

  /// 用于转换 adlog 字段，保存当前节点的 str
  std::vector<std::string> cur_expr_str_;

  std::string common_info_name_ = "";
  int common_info_value_ = 0;
  std::string loop_var_name_ = "";
  std::string expr_type_ = "";
  std::string list_var_name_;
  std::string callee_name_;

  clang::Expr* caller_ = nullptr;
  std::shared_ptr<ExprInfo> caller_info_;

  std::string value_type_;

  /// for map。
  std::string member_str_;
  std::string map_var_name_;
  clang::QualType qual_type_;
  bool is_basic_type_ = true;
  std::string caller_str_;
  std::string map_key_type_;
  std::string map_value_type_;
  std::string map_key_str_;
  std::string map_value_str_;

  std::vector<clang::Expr*> params_;
  std::shared_ptr<ExprInfo> parent_;

  /// 原始的表达式。
  std::string raw_expr_str_;

  std::vector<std::shared_ptr<ExprInfo>> call_expr_params_;

  static std::unordered_set<std::string> ignore_callee_;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
