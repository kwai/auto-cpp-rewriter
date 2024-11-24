#include "../Tool.h"
#include "CommonInfoPrepare.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

CommonInfoPrepare::CommonInfoPrepare(const std::string& prefix_adlog):
  prefix_adlog_(prefix_adlog) {
  prefix_.emplace(tool::adlog_to_bs_enum_str(prefix_adlog));
}

void CommonInfoPrepare::update_size_method_name(const std::string &size_method_name) {
  size_method_name_.emplace(size_method_name);
  if (!method_name_) {
    method_name_.emplace(tool::trim_tail_size(size_method_name));
  }
}

bool CommonInfoPrepare::is_common_info_normal() const {
  return common_info_values_.size() > 0;
}

bool CommonInfoPrepare::is_common_info_fixed_list() const {
  return template_int_names_.size() > 0 && common_info_values_.size() == 0;
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
