#pragma once

#include <glog/logging.h>
#include <absl/types/optional.h>

#include "clang/AST/Expr.h"

#include "RuleBase.h"
#include "../Type.h"
#include "../handler/StrictRewriter.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class Env;

/// 自定义函数 add_feature 处理逻辑。
class AddFeatureMethodRule: public RuleBase {
 public:
  using RuleBase::process;
  explicit AddFeatureMethodRule(clang::Rewriter& rewriter):         // NOLINT
    RuleBase(rewriter, "AddFeatureMethodRule") {}

  void process(clang::CallExpr *call_expr, Env *env_ptr);
  void process(clang::CXXMemberCallExpr* cxx_member_call_expr, Env* env_ptr);
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
