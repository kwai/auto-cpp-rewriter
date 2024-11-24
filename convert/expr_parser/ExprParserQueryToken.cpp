#include <regex>
#include <sstream>
#include <thread>
#include <algorithm>
#include <string>
#include <absl/strings/str_join.h>
#include <absl/strings/str_split.h>

#include "clang/AST/AST.h"

#include "../Deleter.h"
#include "../Tool.h"
#include "../Type.h"
#include "../Env.h"
#include "../ExprInfo.h"
#include "../info/IfInfo.h"
#include "../info/LoopInfo.h"
#include "../info/NewActionParam.h"
#include "../info/NewVarDef.h"
#include "ExprParserQueryToken.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

void update_env_query_token_field_def(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_photo_text_call()) {
    if (expr_info_ptr->call_expr_params_size() == 3) {
      auto param = expr_info_ptr->call_expr_param(2);
      if (param != nullptr && param->is_common_attr_info_enum()) {
        if (absl::optional<int> int_value = param->get_common_attr_int_value()) {
          if (auto feature_info = env_ptr->mutable_feature_info()) {
            // add field def
            std::ostringstream oss;
            oss << "BSFixedCommonInfo<BSMapField<absl::string_view, float>> "
                   "BSGetPhotoText{"
                << tool::add_quote("adlog.item.ad_dsp_info.common_info_attr")
                << ", " << *int_value << "};";
            std::string bs_enum_str = "adlog_item_ad_dsp_info_common_info_attr_key_" +
              std::to_string(*int_value);
            LOG(INFO) << "add photo_text field_def, bs_enum_str: " << bs_enum_str
                      << ", field_def: " << oss.str();
            feature_info->add_field_def(bs_enum_str,
                                        "GetPhotoText",
                                        oss.str(),
                                        NewVarType::MAP,
                                        AdlogVarType::GET_PHOTO_TEXT);
          }
        } else {
          LOG(INFO) << "cannot find int_value from photo text call: " << expr_info_ptr->origin_expr_str();
        }
      }
    }
  }

  // 添加 feature_info 信息。
  if (expr_info_ptr->is_query_token_call()) {
    if (auto feature_info = env_ptr->mutable_feature_info()) {
      feature_info->set_has_query_token(true);
    }
  }
}

void update_env_query_token_loop(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_proto_map_string_float_iter_type()) {
    if (expr_info_ptr->is_from_query_token() || expr_info_ptr->is_from_photo_text()) {
      if (auto& loop_info = env_ptr->cur_mutable_loop_info()) {
        if (loop_info->loop_stage() == LoopStage::INIT) {
          LOG(INFO) << "set_is_query_token: true"
                    << ", loop_var_expr: "
                    << stmt_to_string(expr_info_ptr->get_proto_map_string_float_loop_var_expr());
          loop_info->set_is_query_token_loop(true);
          if (loop_info->loop_var_expr() == nullptr) {
            loop_info->set_loop_var_expr(expr_info_ptr->get_proto_map_string_float_loop_var_expr());
          }
        }
      }
    }
  }
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
