#include <glog/logging.h>
#include "MethodInfo.h"
#include "NewActionParam.h"
#include <absl/types/optional.h>

namespace ks {
namespace ad_algorithm {
namespace convert {

void MethodInfo::update(clang::QualType return_type,
                        const std::string& bs_return_type,
                        const std::string& decl,
                        const std::string& body) {
  return_type_ = return_type;
  bs_return_type_ = bs_return_type;
  decl_ = decl;
  body_ = body;
}

void MethodInfo::add_new_action_param(size_t index,
                                      const NewActionParam &new_action_param,
                                      const std::string& new_origin_name) {
  if (index >= new_action_params_.size()) {
    new_action_params_.resize(index + 1);
  }

  new_action_params_[index] = new_action_param;
  new_action_params_[index].set_origin_name(new_origin_name);
}

void MethodInfo::add_new_action_param(size_t index,
                                      const std::string& origin_name) {
  if (index >= new_action_params_.size()) {
    new_action_params_.resize(index + 1);
  }

  new_action_params_[index] = std::move(NewActionParam(origin_name));
}

void MethodInfo::add_new_action_field_param(const std::string& origin_name,
                                            const std::string& field,
                                            const std::string& inner_type_str,
                                            bool is_combine_feature,
                                            const std::string& new_name) {
  for (size_t i = 0; i < new_action_params_.size(); i++) {
    if (new_action_params_[i].origin_name() == origin_name) {
      if (!new_action_params_[i].has_new_param_name(new_name)) {
        NewActionFieldParam new_param(new_name, field, inner_type_str, is_combine_feature);
        new_action_params_[i].add_new_param(new_param);
      }
    }
  }
}

const NewActionParam& MethodInfo::find_new_action_param(size_t index) const {
  if (index >= new_action_params_.size()) {
    static NewActionParam empty;
    return (empty);
  }

  return new_action_params_[index];
}

absl::optional<std::vector<std::string>> MethodInfo::find_new_action_param_name(size_t index) const {
  if (index >= new_action_params_.size() || new_action_params_[index].new_params().size() == 0) {
    return absl::nullopt;
  }

  std::vector<std::string> new_names;
  const auto& new_field_params = new_action_params_[index].new_params();
  for (size_t i = 0; i < new_field_params.size(); i++) {
    new_names.push_back(new_field_params[i].name());
  }

  return absl::make_optional(new_names);
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
