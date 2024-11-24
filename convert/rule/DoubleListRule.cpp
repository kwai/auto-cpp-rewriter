#include "../Env.h"
#include "../Tool.h"
#include "../ExprParser.h"
#include "../info/NewVarDef.h"
#include "DoubleListRule.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

void DoubleListRule::process(clang::ForStmt* for_stmt,
                             Env* env_ptr) {
}

void DoubleListRule::process(clang::CXXForRangeStmt* cxx_for_range_stmt,
                             Env* env_ptr) {
  if (const auto& loop_info = env_ptr->cur_loop_info()) {
    if (!loop_info->is_double_list_loop()) {
      return;
    }

    // 最多两层 proto list, 更多层不支持。
    // 示例: teams/ad/ad_algorithm/feature/fast/impl/extract_combine_user_app_list_ad_app_id.h
    if (env_ptr->is_parent_loop()) {
      if (!loop_info->is_repeated_common_info() &&
          loop_info->is_proto_list_loop() &&
          !loop_info->is_for_stmt()) {
        auto expr_info_ptr = parse_expr(loop_info->loop_var_expr(), env_ptr);
        if (expr_info_ptr != nullptr) {
          std::ostringstream oss;

          std::string bs_enum_str = expr_info_ptr->get_bs_enum_str();
          if (const absl::optional<NewVarDef>& var = env_ptr->find_new_def(bs_enum_str)) {
            oss << "for (size_t idx = 0; idx < " << var->name() << ".size(); idx++) {"
                << get_loop_body(cxx_for_range_stmt->getBody())
                << "\n}\n";
            rewriter_.ReplaceText(cxx_for_range_stmt, oss.str());
          } else {
            LOG(INFO) << "cannot find loop var def, bs_enum_str: " << bs_enum_str
                      << ", env address: " << env_ptr;
          }
        }
      }
    }

    if (!loop_info->is_repeated_common_info() &&
        loop_info->is_proto_list_loop() &&
        loop_info->is_child_proto_list_loop() &&
        !loop_info->is_for_stmt()) {
      rewriter_.ReplaceText(cxx_for_range_stmt,
                            get_loop_body(cxx_for_range_stmt->getBody()));
      return;
    }
  }
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
