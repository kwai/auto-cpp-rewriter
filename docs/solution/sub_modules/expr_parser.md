# expr_parser

详细实现可参考: `convert/expr_parser.h`。

`expr_parser` 主要用于解析表达式，并将解析得到的信息结合 `ProtoNode` 中的信息，更新到前文所提到的 `Env` 中。
具体的信息回更新到 `Env` 中不同的 `Info` 属性中。用于之后的改写。

实际逻辑也比较多，这里只列出部分关键逻辑。

```cpp
std::shared_ptr<ExprInfo> parse_expr_simple(clang::Expr* expr, Env* env_ptr) {
  if (expr == nullptr) {
    LOG(INFO) << "expr is null";
    return nullptr;
  }

  if (clang::CXXMemberCallExpr* cxx_member_call_expr = dyn_cast<clang::CXXMemberCallExpr>(expr)) {
    auto expr_info_ptr = std::make_shared<ExprInfo>(expr, env_ptr);
    ExprInfo& expr_info = *expr_info_ptr;
    expr_info_ptr->set_raw_expr_str(stmt_to_string(expr));

    clang::Expr* caller = cxx_member_call_expr->getImplicitObjectArgument();
    if (clang::MemberExpr* callee = dyn_cast<clang::MemberExpr>(cxx_member_call_expr->getCallee())) {
      std::string callee_name = callee->getMemberDecl()->getNameAsString();

      expr_info.set_parent(std::move(parse_expr_simple(caller, env_ptr)));

      // for loop, begin is loop var
      if (callee_name == "begin") {
        return expr_info_ptr;
      }

      expr_info.set_callee_name(callee_name);
      expr_info.add_cur_expr_str(callee_name);

      for (unsigned i = 0; i < cxx_member_call_expr->getNumArgs(); i++) {
        expr_info.add_param(cxx_member_call_expr->getArg(i));
        auto param_expr_info_ptr = parse_expr_simple(cxx_member_call_expr->getArg(i), env_ptr);
        param_expr_info_ptr->set_caller_info(expr_info_ptr);
        expr_info_ptr->add_call_expr_param(std::move(param_expr_info_ptr));
      }

      return expr_info_ptr;
    } else {
      std::string expr_str = stmt_to_string(expr);
      LOG(INFO) << "unsupported cxx member call expr: " << expr_str;
      return expr_info_ptr;
    }
  } else if (clang::MemberExpr* member_expr = dyn_cast<clang::MemberExpr>(expr)) {
    ...
  } else if (...) {
    ...
  } else {
    LOG(INFO) << "unknown type, expr: " << stmt_to_string(expr);
    auto expr_info_ptr = std::make_shared<ExprInfo>(expr, env_ptr);
    expr_info_ptr->set_raw_expr_str(stmt_to_string(expr));
    return expr_info_ptr;
  }

  LOG(INFO) << "parse error, expr: " << stmt_to_string(expr);
  return nullptr;
}
```