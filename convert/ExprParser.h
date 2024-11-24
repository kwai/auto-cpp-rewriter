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

/// 解析表达式中基本的信息, 如 callee_nem_, parent_ 等, 用于下一步与 env_ptr 更新参数。
std::shared_ptr<ExprInfo> parse_expr_simple(clang::Expr* expr, Env* env_ptr);

/// 更新 expr 中的各种信息到 env_ptr 中, 用于之后的替换。
std::shared_ptr<ExprInfo> parse_expr(clang::Expr* expr, Env* env_ptr);

void update_env_common_info(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_action_detail(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_action_detail_fixed(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_middle_node(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_double_list(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_get_seq_list(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_proto_list(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_query_token(ExprInfo* expr_info_ptr, Env* env_ptr);
void update_env_general(ExprInfo* expr_info_ptr, Env* env_ptr);

void update_env_bs_field(ExprInfo* expr_info_ptr, Env* env_ptr);

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
