#pragma once

#include <absl/types/optional.h>

#include <vector>
#include <string>
#include <unordered_map>

#include "clang/AST/Type.h"

#include "NewActionParam.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

/// get_value_from_Action 对应的信息，提前写好。
/// 有两个重载版本，通过参数个数可以区分, 一个参数是 8 个，另一个是 10 个，多了两个 photo_id 相关的参数。
/// 对应的 bs_get_value_from_action 实现见:  teams/ad/ad_algorithm/bs_feature/fast/frame/bs_action_util.cc
class ActionMethodInfo {
 public:
  explicit ActionMethodInfo(size_t param_size);

  static const std::string& name() { return (name_); }

  size_t param_size() const { return param_size_; }
  const std::vector<NewActionParam>& new_action_params() const { return (new_action_params_); }

  const NewActionParam& find_param(size_t index) const;

 private:
  static const std::string name_;
  size_t param_size_ = 8;

  /// AdActionInfo 对应到多个用到的字段，需要保存多个新参数。
  std::vector<NewActionParam> new_action_params_;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
