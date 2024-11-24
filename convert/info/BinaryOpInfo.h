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

/// 保存当前 binary op 相关的信息, 如 ==, !=。
/// 用于判断 common info 是等于还是不等于。
class BinaryOpInfo {
 public:
  BinaryOpInfo() = default;
  explicit BinaryOpInfo(const std::string& op,
                         clang::Expr* left_expr,
                         clang::Expr* right_expr):
    op_(op),
    left_expr_(left_expr),
    right_expr_(right_expr) {}

  const std::string& op() const { return op_; }
  clang::Expr* left_expr() const { return left_expr_; }
  clang::Expr* right_expr() const { return right_expr_; }

  bool is_equal_op() const { return op_.find("==") != std::string::npos; }
  bool is_not_equal_op() const { return op_.find("!=") != std::string::npos; }
  bool is_assign_op() const { return op_ == "=" || op_ == "operator="; }
  bool is_greater_op() const { return op_ == ">"; }
  bool is_less_op() const { return op_ == "<"; }
  bool is_less_equal_op() const { return op_ == "<="; }
  bool is_greater_equal_op() const { return op_ == ">="; }
  bool is_and_op() const { return op_ == "&&"; }
  bool is_or_op() const { return op_ == "||"; }

  std::string left_expr_str() const;
  std::string right_expr_str() const;

  ExprType left_expr_type() const { return left_expr_type_; }
  ExprType right_expr_type() const { return right_expr_type_; }

  void set_left_expr_type(ExprType expr_type) { left_expr_type_ = expr_type; }
  void set_right_expr_type(ExprType expr_type) { right_expr_type_ = expr_type; }

 private:
  std::string op_;
  clang::Expr* left_expr_ = nullptr;
  clang::Expr* right_expr_ = nullptr;
  ExprType left_expr_type_ = ExprType::NONE;
  ExprType right_expr_type_ = ExprType::NONE;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
