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

/// hash_fn 相关逻辑。
/// std::hash<std::string> 统一替换为 ad_nn::bs::Hash
class HashFnRule: public RuleBase {
 public:
  using RuleBase::process;
  explicit HashFnRule(clang::Rewriter& rewriter): RuleBase(rewriter, "HashFnRule") {}   // NOLINT

  void process(clang::DeclStmt* decl_stmt, Env* env_ptr) override;
  void process(clang::CXXOperatorCallExpr* cxx_operator_call_expr, Env* env_ptr) override;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
