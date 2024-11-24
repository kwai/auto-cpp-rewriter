#include <regex>
#include <absl/strings/str_join.h>
#include <sstream>
#include "../Env.h"
#include "../Tool.h"
#include "../ExprInfo.h"
#include "../ExprParser.h"
#include "PreRule.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

void PreRule::process(clang::ForStmt* for_stmt, Env* env_ptr) {
  process_deleted_var(env_ptr);
}

void PreRule::process(clang::CXXForRangeStmt* cxx_for_range_stmt, Env* env_ptr) {
  process_deleted_var(env_ptr);
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
