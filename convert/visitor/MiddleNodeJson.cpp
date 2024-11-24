#include <absl/strings/str_split.h>
#include <absl/types/span.h>
#include "../Config.h"
#include "./MiddleNodeJson.h"
#include <string>
#include <sstream>
#include <fstream>

namespace ks {
namespace ad_algorithm {
namespace convert {

MiddleNodeJson::MiddleNodeJson() {
  auto config = GlobalConfig::Instance();

  auto& filename = config->middle_node_json_file;
  std::ifstream f(filename.c_str());

  if (!f.is_open()) {
    LOG(ERROR) << "open file failed! filename: " << filename;
    return;
  }

  data_ = json::parse(f);
  if (data_ == nullptr) {
    LOG(ERROR) << "parse json failed! json_file: " << filename;
    return;
  }

  prefix_map_["photo_info"] = {"adlog.item.ad_dsp_info.photo_info"};
  prefix_map_["live_info"] = {"adlog.item.ad_dsp_info.live_info"};
}

bool MiddleNodeJson::is_inited() const { return data_ != nullptr; }

std::vector<std::string> MiddleNodeJson::find_leaf(const std::string& name) const {
  std::string name_underscore = camel_to_underscore(name);
  std::vector<std::string> name_arr = absl::StrSplit(name_underscore, "_");

  std::vector<std::string> path;
  find_leaf_helper(name_arr, 0, data_, &path);

  std::vector<std::string> leafs = { absl::StrJoin(path, ".")};

  return leafs;
}

void MiddleNodeJson::find_leaf_helper(
  const std::vector<std::string>& name_arr,
  size_t pos,
  const json& info,
  std::vector<std::string>* path_ptr
) const {
  if (path_ptr == nullptr) {
    return;
  }

  if (pos >= name_arr.size()) {
    return;
  }

  for (size_t i = pos; i < name_arr.size(); i++) {
    std::string s = absl::StrJoin(name_arr.data() + pos, name_arr.data() + i + 1, "_");
    if (info.contains(s)) {
      path_ptr->emplace_back(s);
      find_leaf_helper(name_arr, i + 1, info[s], path_ptr);
    } else if (info.contains("children") && info["children"].contains(s)) {
      path_ptr->emplace_back(s);
      find_leaf_helper(name_arr, i + 1, info["children"][s], path_ptr);
    }
  }
}

std::string MiddleNodeJson::camel_to_underscore(const std::string& s) const {
  std::ostringstream oss;

  for (size_t i = 0; i < s.size(); i++) {
    if (std::isupper(s[i])) {
      if (i > 0) {
        oss << "_";
      }
      oss << (char)(std::tolower(s[i]));
    } else if (std::isdigit(s[i])) {
      if (i > 0 && std::isalpha(s[i - 1])) {
        oss << "_";
      }
      oss << s[i];
    } else {
      oss << (char)(s[i]);
    }
  }

  return oss.str();
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
