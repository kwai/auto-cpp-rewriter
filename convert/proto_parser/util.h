#pragma once

#include <nlohmann/json.hpp>
#include <absl/types/optional.h>
#include <absl/strings/match.h>
#include <string>
#include <memory>
#include <utility>
#include <regex>
#include <vector>
#include <unordered_set>
#include <unordered_map>

namespace ks {
namespace ad_algorithm {
namespace proto_parser {

using nlohmann::json;

inline bool is_str_integer(const std::string& s) {
  static const std::regex p(" ?[\\-\\+]?[0-9]+ ?");
  return std::regex_match(s, p);
}

inline bool is_str_starts_with(const std::string& s, const std::string& x) {
  if (s.size() < x.size()) {
    return false;
  }

  return s.substr(0, x.size()) == x;
}

inline bool is_str_ends_with(const std::string& s, const std::string& x) {
  if (s.size() < x.size()) {
    return false;
  }

  return s.substr(s.size() - x.size()) == x;
}

/// TODO(liuzhishan): 和其他地方逻辑有重复，后面统一挪到一个地方。
inline bool is_unified_type_str(const std::string& s) {
  static std::unordered_set<std::string> types = {
      "int",           "int64",           "uint",           "uint64",          "float",
      "str",           "string",          "int_list",       "int64_list",      "uint64_list",
      "uint_list",     "float_list",      "str_list",       "string_list", "map_int64_int64",
      "map_int_int", "map_int_float", "map_int64_float", "map_int_string", "map_int64_string",
      "map_int_bool", "map_int64_bool", "map_uint64_bool", "map_unit64_bool", "bool"};

  return types.find(s) != types.end();
}

inline bool is_nonstd_common_info_enum(const std::string& name) {
  static std::unordered_set<std::string> names = {
    "AUTHOR_HISTORY_REALTIME_PURCHASE_TIMESTAMP_Flag",
    "USER_HISTORY_REALTIME_PURCHASE_TIMESTAMP_Flag",
    "LPS_LLM_LANDING_USER_LPS_1cate_30D",
  };

  if (absl::StartsWith(name, "LPS_LLM_LANDING_USER")) {
    return true;
  }

  return names.find(name) != names.end();
}

}  // namespace proto_parser
}  // namespace ad_algorithm
}  // namespace ks
