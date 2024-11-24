#pragma once

#include <glog/logging.h>

#include <utility>
#include <string>
#include <vector>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/AST/Stmt.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class Env;

using StmtReplacement = std::pair<clang::Stmt*, std::string>;

using StmtReplace = std::function<StmtReplacement(Env*, clang::Stmt*)>;

/// 注意: env_ptr 是局部变量, 使用方应该保证 LazyReplace 初始化与 run 执行的时候 env_ptr 都是合法指针。
/// 比较安全的方式应该是使用 shared_ptr<Env>。
///
/// 不过目前处理的情况比较常见, common info vector 定义的时候初始化 LazyReplace, 遇到调用 int_value()
/// 等 common info 方法时候调用 run, 因此调用时候的声明周期一定是包含在定义时候的声明周期之类的, 直接使用
/// Env* 也可以。
class LazyReplace {
 public:
  explicit LazyReplace(Env* env_ptr, clang::Stmt* stmt, StmtReplace stmt_replace):
    env_ptr_(env_ptr),
    stmt_(stmt),
    stmt_replace_(stmt_replace) {}

  StmtReplacement run() {
    return stmt_replace_(env_ptr_, stmt_);
  }

 private:
  Env* env_ptr_;
  clang::Stmt* stmt_;
  StmtReplace stmt_replace_;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
