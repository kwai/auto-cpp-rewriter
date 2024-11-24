#include <sstream>
#include <string>
#include "ActionDetailInfo.h"
#include "../Tool.h"
#include "../Env.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

absl::optional<int> ActionDetailInfo::first_action() const {
  if (actions_.size() == 0) {
    return absl::nullopt;
  }

  return absl::make_optional(actions_.at(0));
}

ActionDetailInfo::ActionDetailInfo(const std::string& prefix_adlog, int action):
  prefix_adlog_(prefix_adlog) {
  prefix_ =  tool::adlog_to_bs_enum_str(prefix_adlog_);
  actions_.push_back(action);
}

// 如果 iter = action_map.find(no) 存在, 则 it->second.list 一定存在, 那么在 java 转 bslog 的时候
// 一定会添加上 it->second.list.size(), 即 xxx_action_map_list_size 一定存在, 即使其值为 0.
// 因此用 iter->second.list.size() 是否存在来代表 iter != action_map.end()
std::string ActionDetailInfo::get_exists_expr(Env* env_ptr) const {
  absl::optional<std::string> bs_enum_str = get_bs_list_size_enum_str();
  if (!bs_enum_str) {
    LOG(INFO) << "cannot get action detail exists_enum_str, prefix: " << prefix_;
    return "";
  }

  LOG(INFO) << "bs_enum_str: " << *bs_enum_str;
  if (const absl::optional<NewVarDef>& exists_var = env_ptr->find_new_def(*bs_enum_str)) {
    if (exists_var->exists_name().size() > 0) {
      LOG(INFO) << "bs_enum_str: " << *bs_enum_str << ", find exists_name: " << exists_var->exists_name();
      return exists_var->exists_name();
    }
  }

  return "";
}

absl::optional<std::string> ActionDetailInfo::get_bs_enum_str() const {
  if (absl::optional<int> action = first_action()) {
    return absl::optional<std::string>(prefix_ + "_key_" + std::to_string(*action));
  } else {
    return absl::nullopt;
  }
}

absl::optional<std::string> ActionDetailInfo::get_bs_list_size_enum_str() const {
  absl::optional<std::string> bs_enum_str = get_bs_enum_str();
  if (!bs_enum_str) {
    LOG(INFO) << "cannot get bs_enum_str for action detail, prefix: " << prefix_;
    return absl::nullopt;
  }

  return absl::optional<std::string>(*bs_enum_str + "_list_size");
}

std::string ActionDetailInfo::get_action_detail_exists_def(Env* env_ptr) const {
  if (env_ptr == nullptr) {
    LOG(INFO) << "env_ptr is nullptr!";
    return "";
  }

  absl::optional<std::string> bs_enum_str = get_bs_list_size_enum_str();
  if (!bs_enum_str) {
    LOG(INFO) << "cannot get bs_enum_str for action detail exists, prefix: " << prefix_;
    return "";
  }

  std::string var_name = env_ptr->find_valid_new_name(*bs_enum_str);
  std::string new_name = tool::get_exists_name(var_name);
  std::string enum_new_name = tool::get_exists_name(std::string("enum_") + var_name);

  std::ostringstream oss;
  oss << "    auto " << enum_new_name << " = BSFieldEnum::" << *bs_enum_str << ";\n    ";
  oss << "bool " << new_name << " = BSFieldHelper::HasSingular<int64_t";

  if (env_ptr->is_combine_feature() && !tool::is_item_field(*bs_enum_str)) {
    oss << ", true";
  }
  oss << ">"
      << "(*bs, " << enum_new_name << ", pos)";

  return oss.str();
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
