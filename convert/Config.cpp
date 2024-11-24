#include "Config.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

FeatureInfo* GlobalConfig::feature_info_ptr(const std::string& feature_name) {
  std::lock_guard<std::mutex> lock(mu);
  auto it = feature_info.find(feature_name);
  if (it != feature_info.end()) {
    return &(it->second);
  }

  it = feature_info.insert({feature_name, FeatureInfo(feature_name)}).first;
  return &(it->second);
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
