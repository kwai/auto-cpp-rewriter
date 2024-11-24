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
#include "CommonInfoLeaf.h"
#include "CommonInfoPrepare.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class Env;
class StrictRewriter;

/// 来自中间节点的 common info。
/// 示例: teams/ad/ad_algorithm/feature/fast/impl/extract_item_goods_id_list_size.h, 所需字段来自中间
///   节点的 common info, 如 live info common info。
///
///   auto live_info = GetLiveInfo(adlog.item(pos));
///   if (live_info == nullptr) {
///     return;
///   }
///   if (live_info->common_info_attr_size() > 0) {
///     const auto& attr = live_info->common_info_attr();
///     for (const auto& liveAttr : attr) {
///       if (liveAttr.name_value() == 23071) {
///         int goods_num = liveAttr.int_list_value_size();
///         AddFeature(0, goods_num, result);
///         break;
///       }
///     }
///   }
/// 在遇到 `common_info_attr` 时候可以确定是否来自中间节点, 如果是则创建 `CommonInfoMiddleNode`, 之后需要
///   重新实现 `CommonInfo` 对应的逻辑。
class CommonInfoMiddleNodeDetail: public CommonInfoLeaf {
 public:
  explicit CommonInfoMiddleNodeDetail(const std::string& prefix_adlog,
                                      int common_info_value,
                                      const std::string& root);
  explicit CommonInfoMiddleNodeDetail(const std::string& prefix_adlog,
                                      int common_info_value,
                                      const std::string& method_name,
                                      const std::string& root);

  void update_method_name(const std::string& method_name) override {
    if (!is_ready_) {
      CommonInfoCore::update_method_name(method_name);
      is_ready_ = true;
    }
  }

  void update_size_method_name(const std::string& size_method_name) override {
    if (!is_ready_) {
      CommonInfoCore::update_size_method_name(size_method_name);
      LOG(INFO) << "update_size_method: " << size_method_name
                << ", method_name: " << method_name_
                << ", address: " << this;
      is_ready_ = true;
    }
  }

  std::string get_functor_name() const override;

  std::string get_bs_scalar_exists_field_def(Env* env_ptr) const override;

  std::string get_bs_enum_str() const override;
  std::string get_functor_tmpl() const override;
  std::string get_exists_functor_tmpl() const override;
  std::string get_field_def_params() const override;
  bool is_item_field(const std::string& bs_enum_str) const override;

 protected:
  std::string root_;
  std::string bs_rewritten_text_;
  const std::string common_info_camel_ = "CommonInfo";
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
