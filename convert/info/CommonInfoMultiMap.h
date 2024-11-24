#pragma once

#include <glog/logging.h>
#include <absl/types/optional.h>
#include <set>
#include <map>
 #include <string>
#include <unordered_set>

#include "clang/AST/Expr.h"

#include "../Type.h"
#include "CommonInfo.h"
#include "CommonInfoCore.h"
#include "CommonInfoPrepare.h"
#include "PrefixPair.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class Env;
class StrictRewriter;

/// 所有 common info 类型都一样
class CommonInfoMultiMap : public CommonAttrInfo, public CommonInfoCore, public PrefixPair {
 public:
  explicit CommonInfoMultiMap(const std::string& prefix_adlog):
    CommonAttrInfo(CommonInfoType::MULTI_MAP),
    PrefixPair(prefix_adlog) {}

  explicit CommonInfoMultiMap(const std::string& prefix_adlog,
                              const std::string& map_name,
                              const std::string& attr_name):
    CommonAttrInfo(CommonInfoType::MULTI_MAP),
    PrefixPair(prefix_adlog),
    map_name_(map_name),
    attr_name_(attr_name) {}

  const std::string& map_name() const { return map_name_; }
  const std::string& attr_name() const { return attr_name_; }

 private:
  std::string map_name_;
  std::string attr_name_;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
