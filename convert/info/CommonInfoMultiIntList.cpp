#include <sstream>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <string>
#include <glog/logging.h>
#include <unordered_set>
#include <absl/strings/str_split.h>

#include "../Env.h"
#include "../Tool.h"
#include "../Type.h"
#include "CommonInfoMultiIntList.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

void CommonInfoMultiIntList::add_attr_map_name(const std::string &attr_map_name) {
  if (std::find(attr_map_names_.begin(), attr_map_names_.end(), attr_map_name) != attr_map_names_.end()) {
    return;
  }

  attr_map_names_.push_back(attr_map_name);
}

void CommonInfoMultiIntList::add_attr_size_map_name(const std::string &attr_size_map_name) {
  if (std::find(attr_size_map_names_.begin(),
                attr_size_map_names_.end(),
                attr_size_map_name) != attr_size_map_names_.end()) {
    return;
  }

  attr_size_map_names_.push_back(attr_size_map_name);
  if (attr_map_names_.size() > 0) {
    size_list_connections_[attr_size_map_name] = attr_map_names_.back();
  } else {
    LOG(INFO) << "must have attr_map_name before, attr_size_map_name: " << attr_size_map_name;
  }
}

const std::string& CommonInfoMultiIntList::find_correspond_list_map(const std::string& size_map_name) const {
  auto it = size_list_connections_.find(size_map_name);
  if (it == size_list_connections_.end()) {
    static std::string empty;
    LOG(INFO) << "cannot find correspond_list_map, size_map_name: " << size_map_name;
    return (empty);
  }

  return it->second;
}

const std::unordered_map<std::string, std::string>& CommonInfoMultiIntList::map_vec_connections() const {
  return (map_vec_connections_);
}

void CommonInfoMultiIntList::add_map_vec_connection(const std::string& map_name,
                                                    const std::string& vec_name) {
  map_vec_connections_[map_name] = vec_name;
}

std::string CommonInfoMultiIntList::get_bs_list_def(int v) const {
  std::ostringstream oss;

  std::string bs_enum_str = get_bs_enum_str(v);
  std::string new_name = env_ptr_->find_valid_new_name(bs_enum_str);
  std::string type_str = "int64_t";

  oss << "BSRepeatedField<" << type_str;
  if (env_ptr_->is_combine_feature() && !tool::is_item_field(bs_enum_str)) {
    oss << ", true";
  }
  oss << "> " << new_name << " = std::move(" << get_functor_name(v) << "(bs, pos))";

  return oss.str();
}

std::string CommonInfoMultiIntList::get_bs_enum_str(int v) const {
  return prefix_ + "_key_" + std::to_string(v);
}

std::string CommonInfoMultiIntList::get_functor_name(int v) const {
  std::ostringstream oss;

  std::string bs_enum_str = get_bs_enum_str(v);
  std::vector<std::string> arr = absl::StrSplit(bs_enum_str, "_");

  oss << "BSGet";

  bool start = false;
  for (const std::string &s : arr) {
    if (s.find("common") != std::string::npos) {
      start = true;
    }

    if (start) {
      oss << char(toupper(s[0])) << s.substr(1);
    }
  }

  return oss.str();
}

std::string CommonInfoMultiIntList::get_bs_list_field_def(int v) const {
  std::ostringstream oss;

  std::string type_str = "int64_t";
  oss << "BSFixedCommonInfo<BSRepeatedField<" << type_str << ">";
  if (env_ptr_->is_combine_feature() &&
      !tool::is_item_field(get_bs_enum_str(v))) {
    oss << ", true";
  }
  oss << "> " << get_functor_name(v) << "{" << tool::add_quote(prefix_adlog_)
      << ", " << v << "}";

  return oss.str();
}

const std::string& CommonInfoMultiIntList::find_correspond_vec_name(const std::string& map_name) const {
  auto it = map_vec_connections_.find(map_name);
  if (it == map_vec_connections_.end()) {
    static std::string empty;
    return (empty);
  }

  return it->second;
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
