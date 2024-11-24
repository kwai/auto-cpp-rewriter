#include <regex>
#include <sstream>
#include <string>

#include "absl/strings/str_split.h"
#include "../Tool.h"
#include "../Env.h"
#include "CommonInfo.h"
#include "../handler/StrictRewriter.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

void CommonAttrInfo::set_env_ptr(Env* env_ptr) {
  env_ptr_ = env_ptr;
}

Env* CommonAttrInfo::env_ptr() const {
  return env_ptr_;
}

Env* CommonAttrInfo::parent_env_ptr() const {
  if (env_ptr_ == nullptr) {
    return nullptr;
  }

  return env_ptr_->parent();
}

const absl::optional<CommonInfoPrepare>& CommonAttrInfo::get_common_info_prepare() const {
  return env_ptr_->cur_common_info_prepare();
}

const std::string CommonAttrInfo::bool_value = "bool_value";
const std::string CommonAttrInfo::int_value = "int_value";
const std::string CommonAttrInfo::float_value = "float_value";
const std::string CommonAttrInfo::string_value = "string_value";
const std::string CommonAttrInfo::bool_list_value = "bool_list_value";
const std::string CommonAttrInfo::int_list_value = "int_list_value";
const std::string CommonAttrInfo::float_list_value = "float_list_value";
const std::string CommonAttrInfo::string_list_value = "string_list_value";
const std::string CommonAttrInfo::map_unit64_bool_value = "map_unit64_bool_value";
const std::string CommonAttrInfo::map_int64_int64_value = "map_int64_int64_value";
const std::string CommonAttrInfo::map_int64_float_value = "map_int64_float_value";
const std::string CommonAttrInfo::map_int64_string_value = "map_int64_string_value";
const std::string CommonAttrInfo::map_string_int64_value = "map_string_int64_value";
const std::string CommonAttrInfo::map_int64_multi_value = "map_int64_multi_value";
const std::string CommonAttrInfo::common_info_attr_size = "common_info_attr_size";

const std::unordered_set<std::string> CommonAttrInfo::scalar_method_names = {
  bool_value,
  int_value,
  float_value,
  string_value
};

const std::unordered_set<std::string> CommonAttrInfo::list_method_names = {
  bool_list_value,
  int_list_value,
  float_list_value,
  string_list_value
};

const std::unordered_set<std::string> CommonAttrInfo::map_method_names = {
  map_unit64_bool_value,
  map_int64_int64_value,
  map_int64_float_value,
  map_int64_string_value,
  map_string_int64_value
};

const std::unordered_set<std::string> CommonAttrInfo::repeated_size_method_names = {
  common_info_attr_size
};

bool CommonAttrInfo::is_common_info_scalar_method(const std::string& method_name) {
  return scalar_method_names.find(method_name) != scalar_method_names.end();
}

bool CommonAttrInfo::is_common_info_list_method(const std::string& method_name) {
  return list_method_names.find(method_name) != list_method_names.end();
}

bool CommonAttrInfo::is_common_info_map_method(const std::string& method_name) {
  return map_method_names.find(method_name) != map_method_names.end();
}

bool CommonAttrInfo::is_common_info_method(const std::string& method_name) {
  return is_common_info_scalar_method(method_name) ||
    is_common_info_list_method(method_name) ||
    is_common_info_map_method(method_name);
}

bool CommonAttrInfo::is_common_info_size_method(const std::string& method_name) {
  if (ends_with(method_name, "_size")) {
    std::string prefix = method_name.substr(0, method_name.size() - std::string("_size").size());
    if (is_common_info_method(prefix)) {
      return true;
    }
  }

  return false;
}

bool CommonAttrInfo::is_common_info_list_size_method(const std::string& method_name) {
  if (ends_with(method_name, "_size")) {
    std::string prefix = method_name.substr(0, method_name.size() - std::string("_size").size());
    if (is_common_info_list_method(prefix)) {
      return true;
    }
  }

  return false;
}

bool CommonAttrInfo::is_common_info_map_size_method(const std::string& method_name) {
  if (ends_with(method_name, "_size")) {
    std::string prefix = method_name.substr(0, method_name.size() - std::string("_size").size());
    if (is_common_info_map_method(prefix)) {
      return true;
    }
  }

  return false;
}

bool CommonAttrInfo::is_common_info_leaf_method(const std::string& method_name) {
  return is_common_info_method(method_name) || is_common_info_size_method(method_name);
}

bool CommonAttrInfo::is_repeated_common_info_size(const std::string& method_name) {
  return repeated_size_method_names.find(method_name) != repeated_size_method_names.end();
}

absl::optional<CommonInfoValueType> CommonAttrInfo::find_value_type(const std::string& method_name) {
  if (method_name == bool_value) {
    return absl::optional<CommonInfoValueType>(CommonInfoValueType::BOOL);
  } else if (method_name == int_value) {
    return absl::optional<CommonInfoValueType>(CommonInfoValueType::INT);
  } else if (method_name == float_value) {
    return absl::optional<CommonInfoValueType>(CommonInfoValueType::FLOAT);
  } else if (method_name == string_value) {
    return absl::optional<CommonInfoValueType>(CommonInfoValueType::STRING);
  } else if (method_name == bool_list_value) {
    return absl::optional<CommonInfoValueType>(CommonInfoValueType::BOOL_LIST);
  } else if (method_name == int_list_value) {
    return absl::optional<CommonInfoValueType>(CommonInfoValueType::INT_LIST);
  } else if (method_name == float_list_value) {
    return absl::optional<CommonInfoValueType>(CommonInfoValueType::FLOAT_LIST);
  } else if (method_name == string_list_value) {
    return absl::optional<CommonInfoValueType>(CommonInfoValueType::STRING_LIST);
  } else if (method_name == map_unit64_bool_value) {
    return absl::optional<CommonInfoValueType>(CommonInfoValueType::MAP_INT_BOOL);
  } else if (method_name == map_int64_int64_value) {
    return absl::optional<CommonInfoValueType>(CommonInfoValueType::MAP_INT_INT);
  } else if (method_name == map_int64_float_value) {
    return absl::optional<CommonInfoValueType>(CommonInfoValueType::MAP_INT_FLOAT);
  } else if (method_name == map_int64_string_value) {
    return absl::optional<CommonInfoValueType>(CommonInfoValueType::MAP_INT_STRING);
  } else if (method_name == map_string_int64_value) {
    return absl::optional<CommonInfoValueType>(CommonInfoValueType::MAP_STRING_INT);
  } else if (method_name == map_int64_multi_value) {
    return absl::optional<CommonInfoValueType>(CommonInfoValueType::MAP_INT_MULTI_INT);
  } else {
    return absl::nullopt;
  }
}

std::string CommonAttrInfo::get_inner_type_str(CommonInfoValueType value_type) {
  switch (value_type) {
  case CommonInfoValueType::BOOL:
  case CommonInfoValueType::BOOL_LIST:
    return "bool";
  case CommonInfoValueType::INT:
  case CommonInfoValueType::INT_LIST:
  case CommonInfoValueType::MAP_INT_INT:
    return "int64_t";
  case CommonInfoValueType::FLOAT:
  case CommonInfoValueType::FLOAT_LIST:
  case CommonInfoValueType::MAP_INT_FLOAT:
    return "float";
  case CommonInfoValueType::STRING:
  case CommonInfoValueType::STRING_LIST:
  case CommonInfoValueType::MAP_INT_STRING:
    return "absl::string_view";
  case CommonInfoValueType::MAP_STRING_INT:
    return "int64_t";
  default:
    return "";
  }
}

std::pair<std::string, std::string> CommonAttrInfo::get_map_inner_type_str(CommonInfoValueType value_type) {
  switch (value_type) {
  case CommonInfoValueType::MAP_INT_INT:
    return {"int64_t", "int64_t"};
  case CommonInfoValueType::MAP_INT_FLOAT:
    return {"int64_t", "float"};
  case CommonInfoValueType::MAP_INT_STRING:
    return {"int64_t", "absl::string_view"};
  case CommonInfoValueType::MAP_STRING_INT:
    return {"absl::string_view", "int64_t"};
  case CommonInfoValueType::MAP_INT_MULTI_INT:
    return {"int64_t", "int64_t"};
  default:
    return {"", ""};
  }
}

std::string CommonAttrInfo::get_bs_type_str(const std::string& method_name, bool is_combine_user) {
  absl::optional<CommonInfoValueType> value_type = find_value_type(method_name);
  if (!value_type) {
    return "";
  }

  std::ostringstream oss;

  if (is_common_info_scalar_method(method_name)) {
    oss << get_inner_type_str(*value_type);
  } else if (is_common_info_list_method(method_name)) {
    oss << "BSRepeatedField<" << get_inner_type_str(*value_type);
    if (is_combine_user) {
      oss << ", true";
    }
    oss << ">";
  } else {
    std::pair<std::string, std::string> type_str = get_map_inner_type_str(*value_type);
    oss << "BSMapField<" << type_str.first << ", " << type_str.second;
    if (is_combine_user) {
      oss << ", true";
    }
    oss << ">";
  }

  return oss.str();
}

bool CommonAttrInfo::is_common_info_list_or_map_loop(const std::string& s) {
  for (const auto& name : list_method_names) {
    if (s.find(name + "()") != std::string::npos) {
      return true;
    }
    if (s.find(name + "_size()") != std::string::npos) {
      return true;
    }
  }

  for (const auto &name : map_method_names) {
    if (s.find(name + "()") != std::string::npos) {
      return true;
    }
    if (s.find(name + "_size()") != std::string::npos) {
      return true;
    }
  }

  return false;
}

bool CommonAttrInfo::is_common_info_list_or_map_loop(const std::string &s, const std::string& loop_name) {
  std::regex p("for ?\\([^\\(\\)]*" + loop_name + "(.begin\\()? ?\\)");
  std::smatch match_res;
  if (std::regex_search(s, match_res, p)) {
    return true;
  }

  return false;
}

bool CommonAttrInfo::contains_size_method(const std::string& s) {
  for (const auto& x : list_method_names) {
    if (s.find(x + "_size()") != std::string::npos) {
      return true;
    }
  }

  for (const auto& x : map_method_names) {
    if (s.find(x + "_size()") != std::string::npos) {
      return true;
    }
  }

  return false;
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
