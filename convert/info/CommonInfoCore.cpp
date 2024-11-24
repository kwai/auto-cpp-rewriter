#include <regex>
#include <string>

#include "absl/strings/str_split.h"
#include "../Tool.h"
#include "../Env.h"
#include "PrefixPair.h"
#include "CommonInfo.h"
#include "CommonInfoCore.h"
#include "../handler/StrictRewriter.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

CommonInfoCore::CommonInfoCore(const std::string& method_name) {
  update_method_name(method_name);
}

bool CommonInfoCore::is_scalar() const {
  return CommonAttrInfo::is_common_info_scalar_method(method_name_);
}

bool CommonInfoCore::is_list() const {
  return CommonAttrInfo::is_common_info_list_method(method_name_);
}

bool CommonInfoCore::is_map() const {
  return CommonAttrInfo::is_common_info_map_method(method_name_);
}

bool CommonInfoCore::is_list_size() const {
  return CommonAttrInfo::is_common_info_list_size_method(method_name_);
}

bool CommonInfoCore::is_map_size() const {
  return CommonAttrInfo::is_common_info_map_size_method(method_name_);
}

std::string CommonInfoCore::get_list_inner_type_str(CommonInfoValueType value_type) const {
  std::string type_str = CommonAttrInfo::get_inner_type_str(value_type);
  if (list_loop_var_type_ && tool::is_int32_type(*list_loop_var_type_)) {
    return "int32_t";
  } else {
    return type_str;
  }
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
