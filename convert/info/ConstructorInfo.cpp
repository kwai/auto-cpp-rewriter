#include <absl/strings/str_join.h>
#include "AdlogFieldInfo.h"
#include "ConstructorInfo.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

void ConstructorInfo::fix_common_info_enums() {
  for (size_t i = 0; i < common_info_enums_.size(); i++) {
    std::string bs_enum_str = common_info_enums_[i].bs_enum_str();
    if (bs_enum_str.size() > 0 && bs_field_enums_.find(bs_enum_str) != bs_field_enums_.end()) {
      bs_field_enums_.erase(bs_enum_str);
    }
  }
}

std::string ConstructorInfo::joined_params() const {
  return absl::StrJoin(params_, ",");
}

void ConstructorInfo::set_normal_adlog_field_info(const std::string& bs_enum_str,
                                                  const std::string& adlog_field) {
  AdlogFieldInfo adlog_field_info(adlog_field, bs_enum_str, AdlogFieldType::NORMAL);
  adlog_field_infos_.insert({bs_enum_str, adlog_field_info});
}

void ConstructorInfo::set_common_info_field_info(const std::string& bs_enum_str,
                                                 const std::string& adlog_field,
                                                 const std::string& common_info_enum_name,
                                                 int common_info_value) {
  AdlogFieldInfo adlog_field_info(adlog_field, bs_enum_str, AdlogFieldType::COMMON_INFO);
  adlog_field_info.set_common_info_enum_name(common_info_enum_name);
  adlog_field_info.set_common_info_enum_value(common_info_value);
  adlog_field_infos_.insert({bs_enum_str, adlog_field_info});
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
