#include <string>
#include <regex>
#include <algorithm>

#include <glog/logging.h>
#include <gflags/gflags.h>
#include <absl/strings/str_split.h>
#include <absl/strings/str_join.h>

#include "../Tool.h"
#include "IfInfo.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

bool IfInfo::is_check_item_pos_cond() const {
  return is_check_item_pos(if_stmt_);
}

void IfInfo::update_check_equal(const std::string& op) {
  if (op.find("==") != std::string::npos) {
    is_check_equal_ = true;
  } else if (op.find("!=") != std::string::npos) {
    is_check_not_equal_ = true;
  }
}

bool IfInfo::is_body_only_break() const {
  if (if_stmt_ == nullptr) {
    return false;
  }

  std::string body_text = tool::rm_surround_big_parantheses(stmt_to_string(if_stmt_->getThen()));
  std::regex p("[ \\n]*break;[ \\n]*");
  if (std::regex_match(body_text, p)) {
    return true;
  }

  return false;
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
