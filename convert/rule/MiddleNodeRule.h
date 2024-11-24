#pragma once

#include <glog/logging.h>
#include <absl/types/optional.h>

#include "clang/AST/Expr.h"

#include "../Type.h"
#include "RuleBase.h"
#include "../handler/StrictRewriter.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class Env;

class MiddleNodeRule: public RuleBase {
 public:
  using RuleBase::process;
  explicit MiddleNodeRule(clang::Rewriter& rewriter): RuleBase(rewriter, "MiddleNodeRule") {}  // NOLINT

  void process(clang::IfStmt* if_stmt, Env* env_ptr) override;
  void process(clang::ForStmt* for_stmt, Env* env_ptr) override;
  void process(clang::CXXForRangeStmt* cxx_for_range_stmt, Env* env_ptr) override;
  void process(clang::BinaryOperator* binary_operator, Env* env_ptr) override;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
