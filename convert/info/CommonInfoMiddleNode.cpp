#include <regex>
#include <sstream>
#include <string>

#include "absl/strings/str_split.h"
#include "../Tool.h"
#include "../Env.h"
#include "CommonInfo.h"
#include "CommonInfoMiddleNode.h"
#include "MiddleNodeInfo.h"
#include "../handler/StrictRewriter.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

CommonInfoMiddleNodeDetail::CommonInfoMiddleNodeDetail(const std::string& prefix_adlog,
                                                       int common_info_value,
                                                       const std::string& root):
  CommonInfoLeaf(prefix_adlog, common_info_value),
  root_(root) {
}

CommonInfoMiddleNodeDetail::CommonInfoMiddleNodeDetail(const std::string& prefix_adlog,
                                                       int common_info_value,
                                                       const std::string& method_name,
                                                       const std::string& root):
  CommonInfoLeaf(prefix_adlog, common_info_value, method_name),
  root_(root) {
  is_ready_ = true;
}

std::string CommonInfoMiddleNodeDetail::get_functor_name() const {
  std::ostringstream oss;

  std::string bs_enum_str = get_bs_enum_str();
  oss << "BSGet" << root_ << bs_enum_str_to_camel(bs_enum_str);

  return oss.str();
}

std::string CommonInfoMiddleNodeDetail::get_bs_enum_str() const {
  std::string s = prefix_ + "_key_" + std::to_string(common_info_value_);
  return tool::adlog_to_bs_enum_str(s);
}

std::string CommonInfoMiddleNodeDetail::get_functor_tmpl() const {
  return std::string("BS") + root_;
}

std::string CommonInfoMiddleNodeDetail::get_bs_scalar_exists_field_def(Env* env_ptr) const {
  std::ostringstream oss;

  std::string type_str = CommonAttrInfo::get_inner_type_str(value_type_);
  oss << get_exists_functor_tmpl() << "<" << type_str << ">" << " ";
  oss << get_exists_functor_name() << "{" << get_field_def_params() << "}";

  return oss.str();
}

std::string CommonInfoMiddleNodeDetail::get_exists_functor_tmpl() const {
  std::ostringstream oss;
  oss << std::string("BSHas") + root_ + common_info_camel_ + "Impl";
  return oss.str();
}

std::string CommonInfoMiddleNodeDetail::get_field_def_params() const {
  return std::to_string(common_info_value_);
}

bool CommonInfoMiddleNodeDetail::is_item_field(const std::string& bs_enum_str) const {
  return true;
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
