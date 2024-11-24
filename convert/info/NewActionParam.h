#pragma once

#include <absl/types/optional.h>

#include <vector>
#include <string>
#include <unordered_map>

namespace ks {
namespace ad_algorithm {
namespace convert {

class Env;

class NewActionFieldParam {
 public:
  NewActionFieldParam() = default;
  explicit NewActionFieldParam(const std::string& name,
                               const std::string& field,
                               const std::string& inner_type_str,
                               bool is_combine_feature):
    name_(name),
    field_(field),
    inner_type_str_(inner_type_str),
    is_combine_feature_(is_combine_feature) {}

  const std::string& name() const { return name_; }
  const std::string& field() const { return field_; }
  const std::string& inner_type_str() const { return inner_type_str_; }
  bool is_combine_feature() const { return is_combine_feature_; }

  const std::string const_ref_str() const;

  std::string get_bs_enum_str(const std::string& prefix) const;
  std::string get_new_def(const std::string& prefix, Env* env_ptr) const;

 private:
  std::string name_;
  std::string field_;
  std::string inner_type_str_;
  bool is_combine_feature_ = false;
};

class NewActionParam {
 public:
  NewActionParam() = default;
  explicit NewActionParam(const std::string& origin_name): origin_name_(origin_name) {}

  const std::string& origin_name() const { return origin_name_; }
  void set_origin_name(const std::string& origin_name) { origin_name_ = origin_name; }

  void add_new_param(const NewActionFieldParam& new_param);
  const std::vector<NewActionFieldParam>& new_params() const { return (new_params_); }
  const NewActionFieldParam& get_new_param(size_t index) const;

  bool has_new_param_name(const std::string& name) const;
  std::string get_bs_field_param_str() const;

 private:
  std::string origin_name_;
  std::vector<NewActionFieldParam> new_params_;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
