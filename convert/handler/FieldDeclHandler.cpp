#include <regex>
#include <gflags/gflags.h>
#include <glog/logging.h>

#include "../Env.h"
#include "../Tool.h"
#include "../ExprInfo.h"
#include "../ExprParser.h"
#include "FieldDeclHandler.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

void FieldDeclHandler::process(clang::InitListExpr *init_list_expr, Env* env_ptr) {
}

void FieldDeclHandler::process(clang::DeclRefExpr* decl_ref_expr, Env* env_ptr) {
  auto expr_info_ptr = parse_expr(decl_ref_expr, env_ptr);
  if (expr_info_ptr == nullptr) {
    return;
  }

  if (expr_info_ptr->is_item_type_enum()) {
    std::string new_text = std::string("bs::ItemType::") + expr_info_ptr->get_ad_enum_name();
    rewriter_.ReplaceText(decl_ref_expr, new_text);
  }
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
