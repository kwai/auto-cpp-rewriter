#pragma once

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include "../handler/StrictRewriter.h"
#include "clang/AST/Expr.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/Tooling.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

/// 通过 TypeAliasDecl 获取模板参数。
/// 目前主要是获取 common info 模板参数, 保存在 FeatureInfo 中。这一步必须在解析 Extract 之前。
class BSTypeAliasCallback : public clang::ast_matchers::MatchFinder::MatchCallback {
 public:
  BSTypeAliasCallback() = default;

  void run(const clang::ast_matchers::MatchFinder::MatchResult &Result);

  void process_template_param(const clang::TemplateArgument& arg);
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
