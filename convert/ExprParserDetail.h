#pragma once

#include <glog/logging.h>
#include <memory>

#include "clang/AST/Type.h"
#include "clang/AST/Expr.h"

#include "Env.h"
#include "Tool.h"
#include "ExprInfo.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

/// =============== 详细逻辑 ===============

void update_env_common_info_prepare(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_common_info_normal(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_common_info_fixed_list(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_common_info_multi_map(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_common_info_multi_int_list(ExprInfo *expr_info_ptr, Env *env_ptr);

/// is_confirmed 为 false 的时候更新, 一旦 is_confirmed 为 true 则不更新。
void update_env_common_info_prefix(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_common_info_name_value_alias(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_common_info_method_name(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_common_info_int_value(ExprInfo* expr_info_ptr, Env* env_ptr);

void update_env_common_info_normal_detail_with_value(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_common_info_normal_detail_without_value(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_common_info_normal_name_value(ExprInfo* expr_info_ptr, Env* env_ptr);

void update_env_common_info_normal_check_cond_template_int(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_common_info_normal_int_value_in_if(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_common_info_normal_int_value_in_switch(ExprInfo* expr_info_ptr, Env* env_ptr);

void update_env_common_info_normal_enum(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_common_info_normal_detail_def(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_common_info_normal_size_method(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_common_info_normal_list_method(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_common_info_normal_list_method_address(ExprInfo* expr_info_ptr, Env* env_ptr);

/// list_size_method 判断长度不等于, 如
/// if (attr.float_list_value_size() != 32) { ... }
/// if (attr.float_list_value_size() % 3 != 0) { ... }
void update_env_common_info_normal_list_size_method_not_equal(ExprInfo* expr_info_ptr, Env* env_ptr);

/// 自定义 helper method, 替换其中的参数
void update_env_common_info_normal_helper_method(ExprInfo* expr_info_ptr, Env* env_ptr);

/// Extract 函数中遇到 helper 函数时添加定义
void update_env_common_info_normal_helper_def(ExprInfo* expr_info_ptr, Env* env_ptr);

/// 比较 common info map end
void update_env_common_info_normal_map_end_cond(ExprInfo* expr_info_ptr, Env* env_ptr);

/// 可能会有 int64 转 int 的情况。
void update_env_common_info_normal_list_loop_var_type(ExprInfo* expr_info_ptr, Env* env_ptr);

void update_env_common_info_fixed_list_touch(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_common_info_fixed_list_enum_member_ref(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_common_info_fixed_list_size_method(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_common_info_fixed_list_leaf_method(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_common_info_fixed_list_list_method(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_common_info_fixed_list_list_def(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_common_info_fixed_list_map_def(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_common_info_fixed_list_scalar_def(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_common_info_fixed_list_list_method_address(ExprInfo* expr_info_ptr, Env* env_ptr);

void update_env_common_info_multi_map_touch(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_common_info_multi_map_check_cond(ExprInfo* expr_info_ptr, Env* env_ptr);

void update_env_common_info_multi_int_list_add_map(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_common_info_multi_int_list_def(ExprInfo* expr_info_ptr, Env* env_ptr);

/// action_detail 相关逻辑
void update_env_action_detail_prefix(ExprInfo* expr_info_ptr, Env* env_ptr);

void update_env_action_detail_new_def(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_action_detail_check_cond(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_action_detail_action_param_def(ExprInfo* expr_info_ptr, Env* env_ptr);

/// action_detail_fixed 相关逻辑
void update_env_action_detail_fixed_touch(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_action_detail_fixed_var_def(ExprInfo* expr_info_ptr, Env* env_ptr);

/// 中间节点相关逻辑
void update_env_middle_node_root(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_middle_node_leaf_def(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_middle_node_check_cond(ExprInfo* expr_info_ptr, Env* env_ptr);

/// 两层 proto 列表相关的信息。
///
/// GetSeqList 相关逻辑
void update_env_get_seq_list_touch(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_get_seq_list_def(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_get_seq_list_if_cond(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_get_seq_list_loop(ExprInfo* expr_info_ptr, Env* env_ptr);

/// ProtoList 相关逻辑。中间的 proto list。
void update_env_proto_list_leaf(ExprInfo* expr_info_ptr, Env* env_ptr);

/// 更新 proto list size, 可能不在 for 循环中。
/// 示例: teams/ad/ad_algorithm/feature/fast/impl/extract_combine_like_author_id.h
/// const auto & action_detail = adlog.user_info().action_detail();
/// int like_num = action_detail.like().size();
/// for (int i = 0; i < like_num && i < max_like_num; ++i) {
///   ...
/// }
void update_env_proto_list_size(ExprInfo* expr_info_ptr, Env* env_ptr);

/// QueryToken 相关逻辑。
void update_env_query_token_loop(ExprInfo* expr_info_ptr, Env* env_ptr);

/// 普通逻辑
void update_env_general_iter_second(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_general_basic_scalar_def(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_general_str_call(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_general_loop_var_method(ExprInfo* epxr_info_ptr, Env* env_ptr);
void update_env_general_basic_expr(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_general_binary_op_info(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_general_decl_info(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_general_get_norm_query(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_general_loop_var_expr(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_general_int_list_member_loop(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_general_check_item_pos_include(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_general_reco_user_info(ExprInfo* expr_info_ptr, Env* env_ptr);

/// proto map 叶子节点
void update_env_general_proto_map_loop(ExprInfo* expr_info_ptr, Env* env_ptr);

/// proto list size method, 添加定义，可能是次变量最早出现的地方。
void update_env_general_proto_list_size_method(ExprInfo* expr_info_ptr, Env* env_ptr);

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
