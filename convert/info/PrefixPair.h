#pragma once

#include <absl/types/optional.h>

#include <string>

#include "clang/AST/Expr.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class PrefixPair {
 public:
  PrefixPair() = default;
  explicit PrefixPair(const std::string& prefix_adlog);

  const std::string& prefix() const { return prefix_; }
  const std::string& prefix_adlog() const { return prefix_adlog_; }

 protected:
  std::string prefix_;
  std::string prefix_adlog_;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
