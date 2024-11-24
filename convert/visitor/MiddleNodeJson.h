#pragma once

#include <absl/types/span.h>
#include <nlohmann/json.hpp>
#include <absl/types/optional.h>
#include <vector>
#include <string>
#include <unordered_map>

namespace ks {
namespace ad_algorithm {
namespace convert {

using nlohmann::json;

class MiddleNodeJson {
 public:
  static MiddleNodeJson& instance() {
    static MiddleNodeJson x;
    return x;
  }

  bool is_inited() const;

  std::vector<std::string> find_leaf(const std::string& name) const;
  void find_leaf_helper(const std::vector<std::string>& name_arr,
                        size_t pos,
                        const json& info,
                        std::vector<std::string>* path_ptr) const;

 private:
  MiddleNodeJson();
  std::string camel_to_underscore(const std::string& s) const;

 private:
  json data_;
  std::unordered_map<std::string, std::vector<std::string>> prefix_map_;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
