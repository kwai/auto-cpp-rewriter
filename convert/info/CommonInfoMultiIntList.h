#pragma once

#include <absl/types/optional.h>
#include <glog/logging.h>

#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../Type.h"
#include "CommonInfo.h"
#include "CommonInfoCore.h"
#include "CommonInfoPrepare.h"
#include "PrefixPair.h"
#include "clang/AST/Expr.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class Env;
class StrictRewriter;

/// 所有 common info 类型都一样
class CommonInfoMultiIntList : public CommonAttrInfo, public CommonInfoCore, public PrefixPair {
 public:
  explicit CommonInfoMultiIntList(const std::string& prefix_adlog) :
    CommonAttrInfo(CommonInfoType::MULTI_MAP),
    PrefixPair(prefix_adlog) {}

  void add_attr_map_name(const std::string& attr_map_name);
  const std::vector<std::string>& attr_map_names() const { return attr_map_names_; }

  void add_attr_size_map_name(const std::string& attr_size_map_name);
  const std::vector<std::string>& attr_size_map_names() const { return attr_size_map_names_; }

  const std::string& find_correspond_list_map(const std::string& size_map_name) const;

  const std::unordered_map<std::string, std::string>& map_vec_connections() const;
  void add_map_vec_connection(const std::string& map_name, const std::string& vec_name);

  std::string get_bs_list_def(int v) const;
  std::string get_bs_enum_str(int v) const;
  std::string get_functor_name(int v) const;
  std::string get_bs_list_field_def(int v) const;

  const std::string& find_correspond_vec_name(const std::string& map_name) const;

 private:
  std::vector<std::string> attr_map_names_;
  std::vector<std::string> attr_size_map_names_;
  std::unordered_map<std::string, std::string> map_vec_connections_;
  std::unordered_map<std::string, std::string> size_list_connections_;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
