#include <map>
#include <sstream>
#include <absl/types/optional.h>

#include "../Env.h"
#include "../Tool.h"
#include "MiddleNodeInfo.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

std::string MiddleNodeInfo::get_bs_exists_field_def(Env* env_ptr,
                                                    const std::string& leaf,
                                                    const std::string& field) const {
  std::ostringstream oss;
  std::string new_field = tool::trim_exists(field);
  oss << "BSHas" << name_ << "Impl " << leaf << "{" << tool::add_quote(new_field) << "}";

  return oss.str();
}

std::string MiddleNodeInfo::get_root_bs_exists_field_def(Env* env_ptr) const {
  return get_bs_exists_field_def(env_ptr, std::string("BSHas") + name_, "");
}

std::string MiddleNodeInfo::get_bs_scalar_field_def(Env* env_ptr,
                                                    const std::string& leaf,
                                                    const std::string& field,
                                                    clang::QualType qual_type) const {
  std::ostringstream oss;

  // 中间节点都是来自 item。
  std::string type_str = tool::get_builtin_type_str(qual_type);
  oss << "BS" << name_ << "<" << type_str << "> ";
  oss << leaf << "{" << tool::add_quote(field) << "}";

  return oss.str();
}

std::string MiddleNodeInfo::get_bs_list_field_def(Env *env_ptr,
                                                  const std::string &leaf,
                                                  const std::string &field,
                                                  const std::string& inner_type) const {
  std::ostringstream oss;

  // 中间节点都是来自 item。
  oss << "BS" << name_ << "<BSRepeatedField<" << inner_type << ">> ";
  oss << leaf << "{" << tool::add_quote(field) << "}";

  return oss.str();
}

std::string MiddleNodeInfo::get_list_loop_bs_wrapped_text(Env *env_ptr,
                                                          const std::string &body,
                                                          const std::string &bs_enum_str) const {
  if (const auto& var = env_ptr->find_new_def(bs_enum_str)) {
    if (var->name().size() > 0) {
      std::ostringstream oss;
      oss << "for (size_t idx = 0; idx < " << var->name() << ".size(); idx++) {\n    "
          << body
        << "\n}\n    ";
      return oss.str();
    } else {
      LOG(INFO) << "new var name is empty! bs_enum_str: " << bs_enum_str
                << ", middle_node name: " << name_;
      return "";
    }
  }

  LOG(INFO) << "cannot find new var def, bs_enum_str: " << bs_enum_str;
  return "";
}

std::string MiddleNodeInfo::get_bs_list_def(Env *env_ptr,
                                            const std::string& bs_enum_str,
                                            const std::string& middle_node_leaf,
                                            const std::string& type_str) const {
  std::ostringstream oss;

  // middle node 都是来自 item
  std::string name = env_ptr->find_valid_new_name(bs_enum_str);
  oss << "BSRepeatedField<" << type_str << "> " << name << " = std::move(" << middle_node_leaf << "(bs, pos))";

  return oss.str();
}

bool MiddleNodeInfo::is_user_middle_node(const std::string& name) {
  static std::unordered_set<std::string> user_nodes = {
    "FixedCommonInfo"
  };

  return user_nodes.find(name) != user_nodes.end();
}

std::string MiddleNodeInfo::get_bs_str_scalar_def(Env* env_ptr,
                                                  const std::string& bs_enum_str,
                                                  const std::string& middle_node_leaf) const {
  std::ostringstream oss;

  std::string name = env_ptr->find_valid_new_name(bs_enum_str);
  oss << "absl::string_view " << name << " = " << middle_node_leaf << "(bs, pos)";

  return oss.str();
}

std::unordered_map<std::string, std::vector<std::string>> MiddleNodeInfo::possible_adlog_prefix_ = {
    {"PhotoInfo",
     {"adlog.item.ad_dsp_info.photo_info",
      "adlog.item.fans_top_info.photo_info",
      "adlog.item.nature_photo_info.photo_info"}},
    {"PhotoInfoNew",
     {"adlog.item.ad_dsp_info.photo_info",
      "adlog.item.fans_top_info.photo_info",
      "adlog.item.nature_photo_info.photo_info"}},
    {"CommonInfoAttr",
     {"adlog.item.ad_dsp_info.photo_info.common_info_attr",
      "adlog.item.ad_dsp_info.live_info.common_info_attr",
      "adlog.item.fans_top_info.photo_info.common_info_attr",
      "adlog.item.nature_photo_info.photo_info.common_info_attr",
      "adlog.item.fans_top_live_info.live_info.common_info_attr"}},
    {"LiveInfo",
     {"adlog.item.fans_top_live_info.live_info",
      "adlog.item.ad_dsp_info.live_info"}},
    {"AdDspMmuInfo",
     {"adlog.item.ad_dsp_info.ad_dsp_mmu_info"}},
    {"AuthorInfo",
     {"adlog.item.fans_top_live_info.live_info.author_info",
      "adlog.item.fans_top_info.photo_info.author_info",
      "adlog.item.ad_dsp_info.photo_info.author_info",
      "adlog.item.ad_dsp_info.live_info.author_info"}}
};

const std::vector<std::string>& MiddleNodeInfo::get_possible_adlog_prefix(const std::string& root) {
  static std::vector<std::string> empty;
  auto it = possible_adlog_prefix_.find(root);
  if (it == possible_adlog_prefix_.end()) {
    return (empty);
  }

  return (it->second);
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
