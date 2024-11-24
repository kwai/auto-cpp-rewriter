#include "../Tool.h"
#include "./BSFieldInfo.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

void BSFieldInfo::insert_bs_field_enum_var_names(const std::string &bs_var_name,
                                                 const std::vector<std::string> &enum_var_names,
                                                 bool is_has_value_in_params) {
  map_bs_field_detail_[bs_var_name].enum_var_names = enum_var_names;
  map_bs_field_detail_[bs_var_name].is_has_value_in_params = is_has_value_in_params;
}

const std::vector<std::string> &
BSFieldInfo::find_bs_field_enum_var_names(const std::string &bs_var_name) const {
  auto it = map_bs_field_detail_.find(bs_var_name);
  if (it != map_bs_field_detail_.end()) {
    return it->second.enum_var_names;
  }

  static std::vector<std::string> empty;
  return empty;
}

void BSFieldInfo::insert_new_def(
  const std::string &var_name,
  const std::string &new_def,
  NewVarType new_var_type
) {
  auto& x = map_bs_field_detail_[var_name];
  x.new_def = new_def;
  x.new_var_type = new_var_type;
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
