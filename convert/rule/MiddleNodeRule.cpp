#include "../Env.h"
#include "../Tool.h"
#include "../ExprInfo.h"
#include "../ExprParser.h"
#include "MiddleNodeRule.h"
#include <sstream>

namespace ks {
namespace ad_algorithm {
namespace convert {

void MiddleNodeRule::process(clang::IfStmt* if_stmt, Env* env_ptr) {
  if (const auto& if_info = env_ptr->cur_if_info()) {
    if (auto& middle_node_info = env_ptr->mutable_middle_node_info()) {
      // if (if_info->is_check_middle_node_root_cond()) {
      //   std::ostringstream oss;
      //   if (if_info->is_check_equal()) {
      //     oss << "!";
      //   }
      //   oss << "BSHas" << middle_node_info->name() << "(bs, pos)";

      //   rewriter_.ReplaceText(if_stmt->getCond(), oss.str());
      // }
    }
  }
}

void MiddleNodeRule::process(clang::ForStmt* for_stmt, Env* env_ptr) {
}

void MiddleNodeRule::process(clang::CXXForRangeStmt* cxx_for_range_stmt, Env* env_ptr) {
  const auto& loop_info = env_ptr->cur_loop_info();
  if (!loop_info) {
    return;
  }

  if (!loop_info->is_for_stmt() && loop_info->is_middle_node_proto_list_loop()) {
    if (const auto& middle_node_info = env_ptr->get_middle_node_info()) {
      std::string bs_enum_str = loop_info->loop_var_expr_bs_enum_str();
      std::string body = get_loop_body(cxx_for_range_stmt->getBody());
      std::string new_str = middle_node_info->get_list_loop_bs_wrapped_text(env_ptr, body, bs_enum_str);

      rewriter_.ReplaceText(cxx_for_range_stmt, new_str);
    }
  }
}

void MiddleNodeRule::process(clang::BinaryOperator* binary_operator, Env* env_ptr) {
  // 示例: teams/ad/ad_algorithm/feature/fast/impl/extract_photo_enhance_age.h
  // 使用 photo_info 判断是否为空。
  // auto photo_info = GetPhotoInfo(adlog.item(pos));
  // if (photo_info && photo_info->has_photo_attribute()) {
  //   ...
  // }
  //
  // 可能会导致来自 MiddleNode 的 common info name_value() 被访问多次，从而导致 common info value 被添加多次。
  // 需要去重。
  std::string op = binary_operator->getOpcodeStr().str();
  if (const auto &middle_node_info = env_ptr->get_middle_node_info()) {
    if (const auto &if_info = env_ptr->cur_if_info()) {
      if (if_info->if_stage() == IfStage::COND) {
        auto left_expr_info_ptr = parse_expr(binary_operator->getLHS(), env_ptr);
        auto right_expr_info_ptr = parse_expr(binary_operator->getRHS(), env_ptr);

        std::ostringstream oss;
        oss << "BSHas" << middle_node_info->name() << "(bs, pos)";

        if (op == "&&" || op == "||") {
          if (left_expr_info_ptr != nullptr) {
            if (left_expr_info_ptr->is_middle_node_root() &&
                left_expr_info_ptr->is_decl_ref_expr()) {
              LOG(INFO) << "replace left_expr: " << left_expr_info_ptr->origin_expr_str()
                        << ", text: " << oss.str();
              rewriter_.ReplaceText(left_expr_info_ptr->origin_expr(), oss.str());
            }
          }

          if (right_expr_info_ptr != nullptr) {
            if (right_expr_info_ptr->is_middle_node_root() &&
                right_expr_info_ptr->is_decl_ref_expr()) {
              LOG(INFO) << "replace right_expr: " << right_expr_info_ptr->origin_expr_str()
                        << ", text: " << oss.str();
              rewriter_.ReplaceText(right_expr_info_ptr->origin_expr(), oss.str());
            }
          }
        }

        if (op == "==" || op == "!=") {
          if (left_expr_info_ptr != nullptr &&
              left_expr_info_ptr->is_middle_node_root() &&
              left_expr_info_ptr->is_decl_ref_expr() &&
              right_expr_info_ptr != nullptr &&
              right_expr_info_ptr->is_nullptr()) {
            if (op == "==") {
              LOG(INFO) << "replace binary_operator: " << stmt_to_string(binary_operator)
                << ", text: !" << oss.str();
              rewriter_.ReplaceText(binary_operator, std::string("!") + oss.str());
            } else if (op == "!=") {
              LOG(INFO) << "replace binary_operator: " << stmt_to_string(binary_operator)
                        << ", text: " << oss.str();
              rewriter_.ReplaceText(binary_operator, oss.str());
            }
          }
        }
      }
    }
  }
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
