#pragma once

#include <absl/types/optional.h>
#include <map>
#include <string>
#include <unordered_map>
#include "clang/AST/Expr.h"
#include "../Type.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class Env;

/// 保存当前 assign op 相关的信息。
class AssignInfo {
 public:
  AssignInfo() = default;
  explicit AssignInfo(const std::string& name,
                      clang::Expr* left_expr,
                      clang::Expr* right_expr):
    name_(name),
    left_expr_(left_expr),
    right_expr_(right_expr) {}

  const std::string& name() const { return name_; }
  clang::Expr* left_expr() const { return left_expr_; }
  clang::Expr* right_expr() const { return right_expr_; }

 private:
  std::string name_;
  clang::Expr* left_expr_ = nullptr;
  clang::Expr* right_expr_ = nullptr;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
