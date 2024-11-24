#include <regex>
#include <string>

#include "absl/strings/str_split.h"
#include "../Tool.h"
#include "../Env.h"
#include "CommonInfo.h"
#include "CommonInfoDetail.h"
#include "../handler/StrictRewriter.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

CommonInfoDetail::CommonInfoDetail(const std::string& prefix_adlog,
                                   int common_info_value):
  CommonInfoLeaf(prefix_adlog, common_info_value) {
}

CommonInfoDetail::CommonInfoDetail(const std::string& prefix_adlog,
                                   int common_info_value,
                                   const std::string& method_name):
  CommonInfoLeaf(prefix_adlog, common_info_value, method_name) {
  is_ready_ = true;
}

std::string CommonInfoDetail::get_exists_expr(Env* env_ptr) const {
  if (method_name_.size() == 0) {
    LOG(INFO) << "cannot find common info method or value, prefix: " << prefix_;
    return "";
  }

  std::string inner_type_str = CommonAttrInfo::get_inner_type_str(value_type_);
  if (inner_type_str.size() == 0) {
    LOG(INFO) << "cannot get inner_type_str, prefix: " << prefix_
              << ", value_type_: " << static_cast<int>(value_type_);
    return "";
  }

  std::ostringstream oss;
  std::string bs_enum_str = get_bs_enum_str();

  if (name_value_alias_) {
    oss << *name_value_alias_ << " == " << common_info_value_ << " && ";
  }

  LOG(INFO) << "method_name_ : "<< method_name_
            << ", common_info_value: " << common_info_value_
            << ", bs_enum_str: " << bs_enum_str;
  if (const absl::optional<NewVarDef>& var = env_ptr->find_new_def(bs_enum_str)) {
    if (CommonAttrInfo::is_common_info_scalar_method(method_name_)) {
      if (var->exists_name().size() > 0) {
        return var->exists_name();
      } else {
        LOG(INFO) << "cannot find scalar exists var, bs_enum_str: " << bs_enum_str;
      }
    } else {
      if (var->name().size() == 0) {
        LOG(INFO) << "cannot find common info var_name, prefix: " << prefix_;
        return "";
      }

      oss << "!" << var->name() << ".is_empty()";
    }
  }

  return oss.str();
}

std::string CommonInfoDetail::get_bs_enum_str() const {
  return prefix_ + "_key_" + std::to_string(common_info_value_);
}

std::string CommonInfoDetail::get_bs_scalar_def(Env* env_ptr) const {
  if (!is_ready_) {
    LOG(INFO) << "common info is not ready!";
    return "";
  }

  if (env_ptr == nullptr) {
    LOG(INFO) << "env_ptr is nullptr!";
    return "";
  }

  std::string bs_enum_str = get_bs_enum_str();
  std::string new_name = env_ptr->find_valid_new_name(bs_enum_str);
  std::string enum_new_name = std::string("enum_") + new_name;
  std::string type_str = CommonAttrInfo::get_inner_type_str(value_type_);

  std::ostringstream oss;

  oss << "    auto " << enum_new_name << " = BSFieldEnum::" << bs_enum_str << ";\n    ";
  oss << type_str << " " << new_name << " = BSFieldHelper::";
  oss << "GetSingular<" << type_str;

  if (env_ptr->is_combine_feature() && !tool::is_item_field(bs_enum_str)) {
    oss << ", true";
  }
  oss << ">"
      << "(*bs, " << enum_new_name;

  if (!env_ptr->is_user_feature()) {
    oss << ", pos";
  }
  oss << ")";

  return oss.str();
}

std::string CommonInfoDetail::get_bs_scalar_exists_def(Env* env_ptr) const {
  if (!is_ready_) {
    LOG(INFO) << "common info is not ready!";
    return "";
  }

  if (env_ptr == nullptr) {
    LOG(INFO) << "env_ptr is nullptr!";
    return "";
  }

  std::string bs_enum_str = get_bs_enum_str();
  std::string new_name = env_ptr->find_valid_new_name(bs_enum_str);
  std::string enum_new_name = std::string("enum_") + new_name;
  std::string type_str = CommonAttrInfo::get_inner_type_str(value_type_);

  new_name = tool::get_exists_name(new_name);
  enum_new_name = tool::get_exists_name(enum_new_name);

  std::ostringstream oss;

  oss << "    auto " << enum_new_name << " = BSFieldEnum::" << bs_enum_str << ";\n    ";
  oss << "bool " << new_name << " = BSFieldHelper::";
  oss << "HasSingular<" << type_str;

  if (env_ptr->is_combine_feature() && !tool::is_item_field(bs_enum_str)) {
    oss << ", true";
  }
  oss << ">"
      << "(*bs, " << enum_new_name;

  if (!env_ptr->is_user_feature()) {
    oss << ", pos";
  }
  oss << ")";

  return oss.str();
}

std::string CommonInfoDetail::get_bs_list_def(Env* env_ptr) const {
  if (!is_ready_) {
    LOG(INFO) << "common info is not ready!";
    return "";
  }

  if (env_ptr == nullptr) {
    LOG(INFO) << "env_ptr is nullptr!";
    return "";
  }

  std::string bs_enum_str = get_bs_enum_str();
  std::string new_name = env_ptr->find_valid_new_name(bs_enum_str);
  std::string type_str = get_list_inner_type_str(value_type_);
  std::string enum_new_name = std::string("enum_") + new_name;

  std::ostringstream oss;

  oss << "    auto " << enum_new_name << " = BSFieldEnum::" << bs_enum_str << ";\n    ";
  oss << "BSRepeatedField<" << type_str;
  if (env_ptr->is_combine_feature() && !tool::is_item_field(bs_enum_str)) {
    oss << ", true";
  }
  oss << "> " << new_name << "(*bs, " << enum_new_name;
  if (!env_ptr->is_user_feature()) {
    oss << ", pos";
  }
  oss << ")";

  return oss.str();
}

std::string CommonInfoDetail::get_bs_map_def(Env* env_ptr) const {
  if (!is_ready_) {
    LOG(INFO) << "common info is not ready!";
    return "";
  }

  if (env_ptr == nullptr) {
    LOG(INFO) << "env_ptr is nullptr!";
    return "";
  }

  std::string bs_enum_str = get_bs_enum_str();
  std::string new_name = env_ptr->find_valid_new_name(bs_enum_str);
  std::pair<std::string, std::string> map_type_str = CommonAttrInfo::get_map_inner_type_str(value_type_);
  std::string enum_new_name_key = std::string("enum_") + new_name + "_key";
  std::string enum_new_name_value = std::string("enum_") + new_name + "_value";

  std::ostringstream oss;

  oss << "    auto " << enum_new_name_key << " = BSFieldEnum::" << bs_enum_str << "_key" << ";\n    ";
  oss << "    auto " << enum_new_name_value << " = BSFieldEnum::" << bs_enum_str << "_value" << ";\n    ";

  oss << "BSMapField<" << map_type_str.first << ", " << map_type_str.second;
  if (env_ptr->is_combine_feature() && !tool::is_item_field(bs_enum_str)) {
    oss << ", true";
  }
  oss << "> " << new_name << "(*bs, " << enum_new_name_key << ", " << enum_new_name_value;
  if (!env_ptr->is_user_feature()) {
    oss << ", pos";
  }
  oss << ")";

  return oss.str();
}

std::string CommonInfoDetail::get_bs_var_name(Env* env_ptr) const {
  std::string bs_enum_str = get_bs_enum_str();

  if (const absl::optional<NewVarDef>& var = env_ptr->find_new_def(bs_enum_str)) {
    if (is_for_stmt()) {
      // 必定是遍历 list value
      const std::string& loop_var = env_ptr->get_last_loop_var();
      if (loop_var.size() > 0) {
        return var->name() + "Get(" + loop_var + ")";
      } else {
        LOG(INFO) << "cannot find loop var for for_stmt!";
        return "";
      }
    } else {
      return var->name() + ".Get(idx)";
    }
  } else {
    LOG(INFO) << "cannot find list or map var_name in env, bs_enum_str: " << bs_enum_str;
    return "";
  }
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
