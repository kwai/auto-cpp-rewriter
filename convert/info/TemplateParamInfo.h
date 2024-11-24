#pragma once

#include <absl/types/optional.h>
#include <string>
#include "clang/AST/Type.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class TemplateParamInfo {
 public:
  TemplateParamInfo() = default;

  const std::string& name() const { return (name_); }
  void set_name(const std::string& name) { name_ = name; }

  const std::string& value_str() const { return (value_str_); }

  const absl::optional<int>& enum_value() const { return (enum_value_); }
  void set_enum_value(int enum_value) { enum_value_.emplace(enum_value); }

  void set_value_str(const std::string& value_str) {
    value_str_ = value_str;
  }
  void set_value(const std::string& value_str, int enum_value) {
    value_str_ = value_str;
    enum_value_.emplace(enum_value);
  }

  const absl::optional<clang::QualType>& qual_type() const { return (qual_type_); }
  void set_qual_type(clang::QualType qual_type) { qual_type_.emplace(qual_type); }

 private:
  std::string name_;
  std::string value_str_;
  absl::optional<int> enum_value_;
  absl::optional<clang::QualType> qual_type_;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
