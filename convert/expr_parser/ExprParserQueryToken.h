#pragma once

#include <glog/logging.h>

#include <memory>

#include "clang/AST/Type.h"
#include "clang/AST/Expr.h"

#include "../Env.h"
#include "../Tool.h"
#include "../ExprInfo.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

/// QueryToken 相关逻辑。
/// 添加 PhotoText field_def。
void update_env_query_token_field_def(ExprInfo* expr_info_ptr, Env* env_ptr);

void update_env_query_token_loop(ExprInfo* expr_info_ptr, Env* env_ptr);

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
