#pragma once

#include <glog/logging.h>
#include <absl/types/optional.h>

#include <string>
#include <vector>

#include "clang/AST/Expr.h"

#include "../handler/StrictRewriter.h"
#include "RuleBase.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class Env;

class ActionDetailRule: public RuleBase {
 public:
  using RuleBase::process;
  explicit ActionDetailRule(clang::Rewriter& rewriter): RuleBase(rewriter, "ActionDetailRule") {}  // NOLINT

  void process(clang::IfStmt* if_stmt,
               Env* env_ptr) override;

  void process(clang::ForStmt* for_stmt,
               Env* env_ptr) override;

  void process(clang::CXXForRangeStmt* cxx_for_range_stmt,
               Env* env_ptr) override;

  void process(clang::CXXMemberCallExpr* cxx_member_call_expr,
               Env* env_ptr) override;

  void process_action_detail_int_list(clang::SourceRange source_range,
                                      const std::vector<int>& int_list_member_values,
                                      const std::string& body_str,
                                      Env* env_ptr);
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
