#include <set>
#include <map>
#include <regex>
#include <glog/logging.h>
#include <unordered_set>
#include <absl/types/optional.h>

#include "clang/AST/Expr.h"

#include "../Env.h"
#include "Type.h"
#include "../handler/StrictRewriter.h"
#include "CommonInfoFixed.h"
#include "CommonInfoFixedList.h"
#include "CommonInfoMiddleNode.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

void CommonInfoFixedList::add_int_name(const std::string& int_name) {
  if (is_already_exists(int_name)) {
    LOG(INFO) << "int_name already exists, skip, int_name: " << int_name;
    return;
  }

  LOG(INFO) << "add_int_name: " << int_name << ", prefix_adlog_:  " << prefix_adlog_;
  if (middle_node_info_) {
    common_info_details_.emplace_back(new CommonInfoFixed(prefix_adlog_, int_name, middle_node_info_));
  } else {
    common_info_details_.emplace_back(new CommonInfoFixed(prefix_adlog_, int_name));
  }

  if (list_loop_var_) {
    common_info_details_.back()->set_list_loop_var(*list_loop_var_);
  }
}

CommonInfoFixed* CommonInfoFixedList::mutable_common_info_detail_by_int_name(const std::string& int_name) {
  for (size_t i = 0; i < common_info_details_.size(); i++) {
    if (common_info_details_[i] != nullptr && common_info_details_[i]->int_name() == int_name) {
      return common_info_details_[i].get();
    }
  }

  return nullptr;
}

// 目前只能处理 list 类型的 common info, map 类型的还处理不了，不过目前只遇到过 list 的。
std::string CommonInfoFixedList::get_bs_rewritten(StrictRewriter* rewriter_ptr, size_t index) const {
  if (index >= common_info_details_.size()) {
    LOG(INFO) << "out of range, index: " << index
              << ", common_info_details_.size(): " << common_info_details_.size();
    return "";
  }

  if (common_info_details_[index] == nullptr) {
    LOG(INFO) << "common info detail is nullptr, index: " << index
              << ", prefix: " << prefix_;
    return "";
  }

  const CommonInfoFixed& common_info_detail = *(common_info_details_[index]);

  std::ostringstream oss_body;

  if (const absl::optional<CommonInfoPrepare>& common_info_prepare = get_common_info_prepare()) {
    const auto& other_if_stmts = common_info_prepare->other_if_stmt_strs();
    for (size_t i = 0; i < other_if_stmts.size(); i++) {
      oss_body << other_if_stmts[i] << "\n";
    }
  }

  oss_body << "\n" << common_info_detail.get_bs_pre_text() << "\n";
  std::string pre_text = oss_body.str();

  std::string loop_text;
  if (common_info_detail.bs_loop_text()) {
    loop_text = *(common_info_detail.bs_loop_text());
  }

  if (common_info_detail.is_list()) {
    if (common_info_detail.list_loop_var()) {
      std::string var_name = common_info_detail.get_bs_var_name(env_ptr_);
      // std::regex p_list_loop_var(std::string("([^a-zA-Z0-9_])") +
      //                            *(common_info_detail.list_loop_var()) +
      //                            std::string("([^a-zA-Z0-9_])"));
      // loop_text = std::regex_replace(loop_text, p_list_loop_var, std::string("$1") + var_name + std::string("$2"));
      std::string loop_var_assign = std::string("auto ") +
                                    *(common_info_detail.list_loop_var()) +
                                    " = " + var_name;
      loop_text = loop_var_assign + ";\n" + loop_text;
    }
  }


  std::string bs_enum_str = common_info_detail.get_bs_enum_str();
  const absl::optional<NewVarDef>& var = env_ptr_->find_new_def(bs_enum_str);
  if (!var) {
    LOG(INFO) << "cannot find list or map var_name in env, bs_enum_str: " << bs_enum_str;
  }

  std::ostringstream oss;
  oss << "if (" << common_info_detail.get_exists_expr(env_ptr_) << ") {\n    ";

  if (common_info_detail.is_list() && var) {
    if (const auto& compare_list_size_value = common_info_detail.compare_list_size_value()) {
      oss << "if (" << var->name() << ".size()";
      if (const auto& list_size_dividend = common_info_detail.list_size_dividend()) {
        oss << " % " << *list_size_dividend;
      }
      oss << " == " << *compare_list_size_value << ") {\n    ";
    }
  }

  if (common_info_detail.is_scalar()) {
    oss << pre_text;
  } else if (common_info_detail.is_for_stmt()) {
    oss << pre_text << "\n" << loop_text << "\n";
  } else if (common_info_detail.has_list_method_address()) {
    oss << pre_text;
  } else if (common_info_detail.is_list() || common_info_detail.is_map()) {
    if (common_info_detail.size_method_name() && !common_info_detail.is_size_method_in_loop_init()) {
      oss << pre_text;
    } else {
      if (var) {
        oss << pre_text
            << "for (size_t idx = 0; idx < " << var->name() << ".size(); idx++) {\n    "
            << loop_text
            << "\n}\n";
      } else {
        LOG(INFO) << "cannot find list or map var_name in env, bs_enum_str: " << bs_enum_str;
      }
    }
  }

  auto &post_text = common_info_detail.bs_post_text();
  if (post_text) {
    oss << *post_text;
  }

  oss << "}\n\n";

  if (common_info_detail.is_list() && var) {
    if (common_info_detail.compare_list_size_value()) {
      oss << "}\n\n";
    }
  }

  return oss.str();
}

std::string CommonInfoFixedList::get_bs_wrap_text(const std::string& text) const {
  if (common_info_details_.size() == 0) {
    LOG(INFO) << "out of range, index: " << 0
              << ", common_info_details_.size(): " << common_info_details_.size();
    return "";
  }

  if (common_info_details_[0] == nullptr) {
    LOG(INFO) << "common info detail is nullptr, prefix: " << prefix_;
    return "";
  }

  const CommonInfoFixed& common_info_detail = *(common_info_details_[0]);

  std::string s = text;

  if (common_info_detail.is_list()) {
    if (common_info_detail.list_loop_var()) {
      std::string var_name = common_info_detail.get_bs_var_name(env_ptr_);
      // std::regex p_list_loop_var(std::string("([^a-zA-Z0-9_])") +
      //                            *list_loop_var_ +
      //                            std::string("([^a-zA-Z0-9_])"));
      // s = std::regex_replace(s, p_list_loop_var, std::string("$1") + var_name + std::string("$2"));
      std::string loop_var_assign = std::string("auto ") +
        *(common_info_detail.list_loop_var()) +
        " = " + var_name;
      s = loop_var_assign + ";\n" + s;
    }
  }

  std::string bs_enum_str = common_info_detail.get_bs_enum_str();
  const absl::optional<NewVarDef>& var = env_ptr_->find_new_def(bs_enum_str);
  if (!var) {
    LOG(INFO) << "cannot find list or map var_name in env, bs_enum_str: " << bs_enum_str;
  }

  std::ostringstream oss;
  oss << "if (";
  oss << common_info_detail.get_exists_expr(env_ptr_) << ") {\n    ";

  if (common_info_detail.is_list() && var) {
    if (const auto& compare_list_size_value = common_info_detail.compare_list_size_value()) {
      oss << "if (" << var->name() << ".size()";
      if (const auto& list_size_dividend = common_info_detail.list_size_dividend()) {
        oss << " % " << *list_size_dividend;
      }
      oss << " == " << *compare_list_size_value << ") {\n    ";
    }
  }

  if (common_info_detail.is_scalar() ||
      common_info_detail.is_list_size() ||
      common_info_detail.is_map_size()) {
    oss << s;
  } else if (common_info_detail.is_for_stmt()) {
    oss << s;
  } else if (common_info_detail.has_list_method_address()) {
    oss << s;
  } else if (common_info_detail.is_list() || common_info_detail.is_map()) {
    if (common_info_detail.size_method_name() && !common_info_detail.is_size_method_in_loop_init()) {
      oss << s;
    } else {
      std::string bs_enum_str = common_info_detail.get_bs_enum_str();
      if (const absl::optional<NewVarDef>& var = env_ptr_->find_new_def(bs_enum_str)) {
        oss << "for (size_t idx = 0; idx < " << var->name() << ".size(); idx++) {\n    "
            << s
            << "\n}\n";
      } else {
        LOG(INFO) << "cannot find list or map var_name in env, bs_enum_str: " << bs_enum_str;
      }
    }
  }

  oss << "}\n\n";

  if (common_info_detail.is_list() && var) {
    if (common_info_detail.compare_list_size_value()) {
      oss << "}\n\n";
    }
  }

  return oss.str();
}

bool CommonInfoFixedList::is_already_exists(const std::string& int_name) {
  for (size_t i = 0; i < common_info_details_.size(); i++) {
    if (common_info_details_[i]->int_name() == int_name) {
      return true;
    }
  }

  return false;
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
