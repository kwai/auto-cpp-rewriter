#include <regex>
#include <string>
#include <sstream>
#include <absl/strings/str_split.h>
#include "../Env.h"
#include "../Tool.h"
#include "ActionDetailFixedInfo.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

ActionDetailFixedInfo::ActionDetailFixedInfo(const std::string& prefix_adlog,
                                             const std::string& action):
  prefix_adlog_(prefix_adlog), action_(action) {
  prefix_ = tool::adlog_to_bs_enum_str(prefix_adlog_);
}

std::string ActionDetailFixedInfo::get_exists_expr(Env* env_ptr,
                                                   const std::string& field_name) const {
  return get_exists_functor_name(field_name) + std::string("(bs, pos)");
}

std::string ActionDetailFixedInfo::get_exists_field_def(Env* env_ptr,
                                                        const std::string& field_name) const {
  std::ostringstream oss;

  oss << "BSHasFixedActionDetailImpl<" << user_template_param(env_ptr, field_name) << "> "
      << get_exists_functor_name(field_name) << "{"
      << tool::add_quote(prefix_adlog_) << ", "
      << action_ << ", "
      << tool::add_quote(tool::trim_exists(field_name))
      << "}";

  return oss.str();
}

std::string ActionDetailFixedInfo::get_action_detail_exists_expr(Env* env_ptr) const {
  return get_exists_expr(env_ptr, "list.size");
}

std::string ActionDetailFixedInfo::get_action_detail_exists_field_def(Env* env_ptr) const {
  return get_exists_field_def(env_ptr, "list.size");
}

std::string ActionDetailFixedInfo::get_bs_enum_str(const std::string& field_name) const {
  std::string s = prefix_ + "_key_" + action_ + "_" + field_name;
  return tool::adlog_to_bs_enum_str(s);
}

std::string ActionDetailFixedInfo::get_bs_var_name(Env* env_ptr,
                                                    const std::string& field_name) const {
  std::string bs_enum_str = get_bs_enum_str(field_name);

  if (const absl::optional<NewVarDef>& var = env_ptr->find_new_def(bs_enum_str)) {
    // 必定是 for_stmt 遍历 list value
    const std::string& loop_var = env_ptr->get_last_loop_var();
    if (loop_var.size() > 0) {
      return var->name() + "Get(" + loop_var + ")";
    } else {
      LOG(INFO) << "cannot find loop var for for_stmt!";
      return "";
    }
  } else {
    LOG(INFO) << "cannot find list or map var_name in env, bs_enum_str: " << bs_enum_str;
    return "";
  }
}

std::string ActionDetailFixedInfo::get_bs_list_def(Env* env_ptr,
                                                   const std::string& field_name,
                                                   clang::QualType qual_type) const {
  std::ostringstream oss;

  std::string bs_enum_str = get_bs_enum_str(field_name);
  std::string new_name = env_ptr->find_valid_new_name(bs_enum_str);

  oss << "BSRepeatedField<"
      << user_template_param(env_ptr, field_name, tool::get_builtin_type_str(qual_type)) << "> "
      << new_name << " = std::move("
      << get_functor_name(field_name) << "(bs, pos))";

  return oss.str();
}

std::string ActionDetailFixedInfo::get_bs_list_field_def(Env* env_ptr,
                                                         const std::string& field_name,
                                                         clang::QualType qual_type) const {
  std::ostringstream oss;

  oss << "BSFixedActionDetail<" << user_template_param(env_ptr, field_name) << ">"
      << get_functor_name(field_name) << "{"
      << tool::add_quote(prefix_adlog_) << ","
      << action_ << ","
      << tool::add_quote(field_name)
      << "}";

  return oss.str();
}

std::string ActionDetailFixedInfo::get_functor_name(const std::string& field_name) const {
  std::ostringstream oss;

  std::vector<std::string> arr = absl::StrSplit(prefix_adlog_, ".");
  oss << arr.back() << "." << field_name;

  return tool::dot_underscore_to_camel(oss.str());
}

std::string ActionDetailFixedInfo::get_exists_functor_name(const std::string& field_name) const {
  std::string functor_name = get_functor_name(field_name);
  static std::regex p("BSGet");
  return std::regex_replace(functor_name, p, "BSHas");
}

std::string ActionDetailFixedInfo::user_template_param(Env* env_ptr,
                                                       const std::string& field_name) const {
  if (env_ptr->is_combine_feature() && !tool::is_item_field(get_bs_enum_str(field_name))) {
    return "true";
  }
  return "";
}

std::string ActionDetailFixedInfo::user_template_param(Env* env_ptr,
                                                       const std::string& field_name,
                                                       const std::string& type_str) const {
  std::ostringstream oss;
  oss << type_str;
  if (env_ptr->is_combine_feature() && !tool::is_item_field(get_bs_enum_str(field_name))) {
    oss << ", true";
  }

  return oss.str();
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
