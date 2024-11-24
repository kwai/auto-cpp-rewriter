#include <string>
#include <sstream>
#include <glog/logging.h>

#include <absl/strings/str_join.h>

#include "../Env.h"
#include "../Tool.h"
#include "NewActionParam.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

// action detail 肯定是 user 特征
const std::string NewActionFieldParam::const_ref_str() const {
  std::ostringstream oss;
  oss << "const BSRepeatedField<" << inner_type_str_;
  if (is_combine_feature_) {
    oss << ", true";
  }
  oss << ">& " << name_;

  return oss.str();
}

std::string NewActionFieldParam::get_bs_enum_str(const std::string& prefix) const {
  return prefix + "_" + field_;
}

std::string NewActionFieldParam::get_new_def(const std::string& prefix, Env* env_ptr) const {
  std::ostringstream oss;

  bool is_user = !tool::is_item_feature(prefix);
  std::string bs_enum_str = get_bs_enum_str(prefix);
  std::string var_name = env_ptr->find_valid_new_name(bs_enum_str);

  oss << "auto enum_" << var_name << " = BSFieldEnum::" << bs_enum_str << ";\n    ";
  if (field_ == "size") {
    oss << inner_type_str_ << " " << var_name << " = BSFieldHelper::GetSingular<" << inner_type_str_;
    if (is_combine_feature_ && is_user) {
      oss << ", true";
    }
    oss << ">" << "(*bs, enum_" << var_name << ", pos);";
  } else {
    oss << "BSRepeatedField<" << inner_type_str_;
    if (is_combine_feature_ && is_user) {
      oss << ", true";
    }
    oss << "> " << var_name << "(*bs, enum_" << var_name << ", pos);";
  }

  return oss.str();
}

void NewActionParam::add_new_param(const NewActionFieldParam& new_param) {
  new_params_.push_back(new_param);
}

const NewActionFieldParam& NewActionParam::get_new_param(size_t index) const {
  if (index >= new_params_.size()) {
    static NewActionFieldParam empty;
    return (empty);
  }

  return (new_params_[index]);
}

bool NewActionParam::has_new_param_name(const std::string& name) const {
  for (size_t i = 0; i < new_params_.size(); i++) {
    if (new_params_[i].name() == name) {
      return true;
    }
  }

  return false;
}

std::string NewActionParam::get_bs_field_param_str() const {
  std::vector<std::string> arr;
  for (size_t i = 0; i < new_params_.size(); i++) {
    arr.push_back(new_params_[i].name());
  }

  return absl::StrJoin(arr, ",");
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
