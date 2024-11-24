#include <string>

#include "ExprInfo.h"
#include "Env.h"
#include "Tool.h"
#include "Deleter.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

// 大部分来自 adlog 的变量定义都需要删除, 因为会在 Env 中单独添加 enum 以及定义。
bool Deleter::need_delete(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr == nullptr) {
    return false;
  }

  // 中间节点的叶子节点普通变量不需要删除, 直接把值替换为中间节点的 bs 表达式。
  // 其他节点定义需要删除。
  // 如
  //    const auto &author_attr = live_info->author_info().attribute();
  //    uint32 fans_count = author_attr.fans_count();
  // 替换为
  //    uint32 fans_count = bs_util.BSGetLiveInfoAuthorInfoAttributeFansCount(bs, pos);
  //
  //  author_attr 被删除, 而 fans_count 被保留。
  if (expr_info_ptr->is_basic() && expr_info_ptr->is_from_middle_node()) {
    return false;
  }

  // 叶子节点的 size 方法不需要删除。
  if (expr_info_ptr->callee_name() == "size" && expr_info_ptr->is_in_decl_stmt()) {
    return false;
  }

  // 引用其他变量赋值不需要被删除。
  if (expr_info_ptr->is_basic() &&
      expr_info_ptr->is_decl_init_expr() &&
      expr_info_ptr->is_decl_ref_expr()) {
    return false;
  }

  // 示例:
  // // const auto& ad_action = adlog.user_info().explore_long_term_ad_action();
  // // auto iter = ad_action.find(no);
  // // const auto& action_base_infos = iter->second.list();
  // if (expr_info_ptr->is_from_adlog() && !expr_info_ptr->is_basic()) {
  //   return true;
  // }

  if (expr_info_ptr->is_reco_proto_type()) {
    return false;
  }

  // 引用 `seq_list` 类型需要删除。
  //
  // 示例:
  // const auto& seq_list = *seq_list_ptr;
  if (expr_info_ptr->is_seq_list_root_deref()) {
    return true;
  }

  // 字符串拼接不需要删除。
  //
  // 示例:
  // std::string const &province = loc.province();
  // std::string const &city = loc.city();
  // std::string res = province + city + community_type;
  if (expr_info_ptr->is_str_concat()) {
    return false;
  }

  // photo token 方法不需要删除。
  if (expr_info_ptr->is_query_token_call() || expr_info_ptr->is_photo_text_call()) {
    return false;
  }

  // CommonInfoMultiMap 中的 int_list_value 方法不删除。
  // 示例:
  // filename: ad/ad_algorithm/feature/fast/impl/extract_combine_realtime_action_match_cnt_v2.h
  //
  // auto id = action_list[key_idx].int_list_value(i);
  if (expr_info_ptr->is_basic()) {
    if (const auto& decl_info = env_ptr->cur_decl_info()) {
      if (expr_info_ptr->is_common_info_list_method()) {
        if (auto feature_info = env_ptr->get_feature_info()) {
          if (feature_info->has_common_info_multi_map()) {
            return false;
          }
        }
      }
    }
  }

  if (expr_info_ptr->is_from_seq_list()) {
    return false;
  }

  if (expr_info_ptr->is_from_seq_list_reco()) {
    return false;
  }

  if (expr_info_ptr->is_from_reco_user_info()) {
    return false;
  }

  if (expr_info_ptr->is_common_info_multi_map_attr()) {
    return false;
  }

  if (expr_info_ptr->is_from_adlog()) {
    return true;
  }

  return false;
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
