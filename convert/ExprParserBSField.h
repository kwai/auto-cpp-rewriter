#pragma once

#include <glog/logging.h>
#include <memory>

#include "clang/AST/Type.h"
#include "clang/AST/Expr.h"

#include "./Env.h"
#include "./Tool.h"
#include "./ExprInfo.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

/// 将新的 bs field 变量定义添加到新的 env 中，老的 decl_stmt 需要删掉。
void update_env_bs_field_decl(ExprInfo* expr_info_ptr, Env* env_ptr);

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
