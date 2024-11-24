#pragma once

#include <absl/types/optional.h>
#include <map>
#include <string>
#include <unordered_map>
#include "../Type.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class Env;

/// adlog field 类型。
/// 1. 普通字段，如 adlog.user_info.id
/// 2. CommonInfo, 如 adlog.user_info.common_info_attr.key:239
/// 3. MiddleNode, 如 photo_info->author_info().id()
enum class AdlogFieldType {
  NONE,
  NORMAL,
  COMMON_INFO,
  MIDDLE_NODE
};

/// 保存当前 adlog field 相关的信息。包括以下几种:
/// 1. 普通字段，包含 adlog_field, bs_enum_str
/// 2. CommonInfo，包含 adlog_field, bs_enum_str, enum_value, enum_str
/// 3. MiddleNode, 包含 adlog_field, bs_enum_str, root
class AdlogFieldInfo {
 public:
  AdlogFieldInfo() = default;
  explicit AdlogFieldInfo(const std::string& adlog_field,
                          const std::string& bs_enum_str,
                          AdlogFieldType adlog_field_type):
    adlog_field_(adlog_field), bs_enum_str_(bs_enum_str), adlog_field_type_(adlog_field_type) {}

  const std::string& adlog_field() const { return (adlog_field_); }
  const std::string& bs_enum_str() const { return (bs_enum_str_); }
  AdlogFieldType adlog_field_type() const { return adlog_field_type_; }
  const absl::optional<std::string>& common_info_enum_name() const { return (common_info_enum_name_); }
  const absl::optional<int>& common_info_enum_value() const { return (common_info_enum_value_); }

  void set_common_info_enum_name(const std::string& common_info_enum_name) {
    common_info_enum_name_.emplace(common_info_enum_name);
  }

  void set_common_info_enum_value(int common_info_enum_value) {
    common_info_enum_value_.emplace(common_info_enum_value);
  }

 private:
  std::string adlog_field_;
  std::string bs_enum_str_;
  AdlogFieldType adlog_field_type_;
  absl::optional<std::string> common_info_enum_name_;
  absl::optional<int> common_info_enum_value_;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
