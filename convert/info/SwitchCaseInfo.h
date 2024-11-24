#pragma once

#include <absl/types/optional.h>

#include <map>

#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/AST/AST.h"
#include "clang/AST/StmtCXX.h"
#include "clang/AST/ASTConsumer.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

enum class SwitchCaseStage {
  COND,
  BODY
};

/// 保存 if 相关的信息
class SwitchCaseInfo {
 public:
  SwitchCaseInfo() = default;

  explicit SwitchCaseInfo(clang::CaseStmt* case_stmt):
  case_stmt_(case_stmt),
    switch_case_stage_(SwitchCaseStage::COND) {}

  clang::CaseStmt* case_stmt() const { return case_stmt_; }

  void set_switch_case_stage(SwitchCaseStage switch_case_stage) { switch_case_stage_ = switch_case_stage; }
  SwitchCaseStage switch_case_stage() const { return switch_case_stage_; }

  void set_common_info_index(size_t index) { common_info_index_.emplace(index); }
  const absl::optional<size_t>& common_info_index() const { return (common_info_index_); }

  void set_common_info_value(int value) { common_info_value_.emplace(value); }
  const absl::optional<int>& common_info_value() const { return (common_info_value_); }

 private:
  clang::CaseStmt* case_stmt_ = nullptr;
  SwitchCaseStage switch_case_stage_;
  absl::optional<size_t> common_info_index_;
  absl::optional<int> common_info_value_;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
