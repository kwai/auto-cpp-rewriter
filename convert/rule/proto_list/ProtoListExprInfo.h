#pragma once

#include <string>
#include <vector>
#include <set>
#include <list>
#include <unordered_set>

#include "../../ExprInfo.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class Env;

class ProtoListExprInfo: public ExprInfo {
 public:
  using ExprInfo::ExprInfo;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
