#pragma once

#include <absl/types/optional.h>

#include <string>
#include <vector>
#include <unordered_map>
#include "clang/AST/Expr.h"
#include "../Type.h"
#include "NewVarDef.h"
#include "PrefixPair.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class ProtoListInfo : public PrefixPair {
 public:
  explicit ProtoListInfo(const std::string& prefix_adlog): PrefixPair(prefix_adlog) {}

  const std::vector<std::string>& fields() const { return (fields_); }
  void add_field(const std::string& field) { fields_.push_back(field); }

 private:
  std::vector<std::string> fields_;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
