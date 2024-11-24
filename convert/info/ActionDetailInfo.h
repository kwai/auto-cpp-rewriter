#pragma once

#include <absl/types/optional.h>

#include <map>
#include <string>
#include <vector>
#include <unordered_map>

#include "clang/AST/Expr.h"

#include "InfoBase.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class Env;

/// action detail 分为以下几种情况:
/// 1. 单个 action_detail
/// 保存行为信息的 map, 通常以行为对应的枚举为 key, value 为一个 proto Message, 里面有变量 list, list 中保存了
/// 行为。因此最终叶子节点的行为是展开的 list。
///
/// 如: teams/ad/ad_algorithm/feature/fast/impl/extract_user_dense_ad_item_click_num.h
///
/// const auto& ad_action = adlog.user_info().explore_long_term_ad_action();
/// auto iter = ad_action.find(no);
/// if (iter != ad_action.end()) {
///  const auto& action_base_infos = iter->second.list();
///  int64_t imp_num = action_base_infos.size();
///  AddFeature(0, imp_num, result);
/// } else {
///  AddFeature(0, 0, result);
/// }
///
/// 2. 多个 action_detail
/// 多个 action_detail, 但是在构造函数中可以确定具体的 action, 而不是通过模板参数传递，与
/// ActionDetailFixedInfo 有区别。
///
/// 如: teams/ad/ad_algorithm/feature/fast/impl/extract_match_dense_num.h,
///    action_vec_ 是 std::vector<int> 类型, 构造函数中会进行初始化。
///
/// const auto& ad_action = adlog.user_info().explore_long_term_ad_action();
/// for (auto action_no : action_vec_) {
///   auto action_no_iter = ad_action.find(action_no);
///   int photo_id_action_num = 0;
///   int product_name_hash_action_num = 0;
///  int second_industy_id_hash_action_num = 0;
///   if (action_no_iter != ad_action.end()) {
///     const auto& action_no_list = action_no_iter->second.list();
///    for (int k = 0; k < action_no_list.size() && k < 100; ++k) {
///       ...
///     }
///  }
///
/// 处理逻辑:
/// 由于 action 在构造函数中都可以知道, 因此需要在 LoopInfo 中将这几个值保存起来, 然后在遍历 action_vec_ 时
/// 即可确定当前类的逻辑是 ActionDetailMultiInfo, 将 action_no 在 Env 中设置为第一个值, 然后在处理 action_vec_
/// for 循环时候先或的 body 的结果，再将 `_key_x_` 统一替换为其他的 action, 从而得到多个 action 的结果。
///
/// 3. action no 通过模板参数传递, 参考 ActionDetailFixedInfo
class ActionDetailInfo : public InfoBase {
 public:
  ActionDetailInfo() = default;
  explicit ActionDetailInfo(const std::string& prefix_adlog, int action);

  const std::string& prefix() const { return prefix_; }
  const std::string& prefix_adlog() const { return prefix_adlog_; }

  absl::optional<int> first_action() const;
  void add_action(int action) { actions_.push_back(action); }

  const std::vector<std::string>& leaf_fields() const { return leaf_fields_; }

  std::string get_exists_expr(Env* env_ptr) const;
  absl::optional<std::string> get_bs_enum_str() const;
  absl::optional<std::string> get_bs_list_size_enum_str() const;

  std::string get_action_detail_exists_def(Env* env_ptr) const;

 private:
  std::string prefix_;
  std::string prefix_adlog_;
  std::vector<int> actions_;
  std::vector<std::string> leaf_fields_;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
