#include "../Env.h"
#include "../Tool.h"
#include "../ExprParser.h"
#include "../info/NewVarDef.h"
#include "ProtoListRule.h"
#include <sstream>

namespace ks {
namespace ad_algorithm {
namespace convert {

void ProtoListRule::process(clang::ForStmt* for_stmt, Env* env_ptr) {
}

void ProtoListRule::process(clang::CXXForRangeStmt* cxx_for_range_stmt, Env* env_ptr) {
  if (const auto& loop_info = env_ptr->cur_loop_info()) {
    if (loop_info->is_double_list_loop()) {
      return;
    }

    if (const auto& proto_list_info = env_ptr->cur_proto_list_info()) {
      std::string bs_enum_str = proto_list_info->prefix();

      const auto& fields = proto_list_info->fields();
      if (fields.size() > 0) {
        std::string adlog_str = proto_list_info->prefix_adlog() + "." + fields[0];
        bs_enum_str = tool::adlog_to_bs_enum_str(adlog_str);
      }

      if (const auto& var = env_ptr->find_new_def(bs_enum_str)) {
        std::ostringstream oss;

        oss << "for (size_t idx = 0; idx < " << var->name() << ".size(); idx++) {\n    "
          << "  auto " << loop_info->loop_var() << " = " << var->name() << ".Get(idx);\n    "
            << get_compound_stmt_rewritten_text(cxx_for_range_stmt->getBody())
            << "\n}\n    ";

        rewriter_.ReplaceText(cxx_for_range_stmt, oss.str());
      } else {
        LOG(INFO) << "cannot find var def in env, bs_enum_str: " << bs_enum_str;
      }
    }
  }
}

void ProtoListRule::process(clang::CXXMemberCallExpr* cxx_member_call_expr, Env* env_ptr) {
  auto expr_info_ptr = parse_expr(cxx_member_call_expr, env_ptr);
  if (expr_info_ptr == nullptr) {
    return;
  }

  // 比较特殊。for 循环遍历的时候还不知道具体类型，因此没办法添加定义，只能根据 bs_enum_str 获取
  // 变量名，然后替换为 var_name.size(), 在 for 循环内部才会添加定义。
  if (expr_info_ptr->is_general_proto_list_size_method()) {
    if (!env_ptr->is_in_for_range_init()) {
      std::string bs_enum_str = expr_info_ptr->get_bs_enum_str_trim_size();
      std::string var_name = env_ptr->find_valid_new_name(bs_enum_str);
      if (var_name.size() > 0) {
        rewriter_.ReplaceText(cxx_member_call_expr, var_name + ".size()");
      }
    }
  }
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
