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

/// 在其他 Rule 之前执行，如 process_deleted_var
class PreRule: public RuleBase {
 public:
  using RuleBase::process;
  explicit PreRule(clang::Rewriter& rewriter): RuleBase(rewriter, "PreRule") {}  // NOLINT

  void process(clang::ForStmt* for_stmt, Env* env_ptr) override;
  void process(clang::CXXForRangeStmt* cxx_for_range_stmt, Env* env_ptr) override;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
