#include <absl/strings/str_join.h>
#include <absl/strings/str_split.h>
#include <absl/types/optional.h>

#include <algorithm>
#include <regex>
#include <sstream>
#include <string>
#include <thread>

#include "clang/AST/AST.h"
#include "./Deleter.h"
#include "./ExprParserBSField.h"
#include "./Tool.h"
#include "./Type.h"
#include "./info/IfInfo.h"
#include "./info/LoopInfo.h"
#include "./info/NewVarDef.h"
#include "./info/BSFieldInfo.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

using tool::strip_suffix_semicolon_newline;

void update_env_bs_field_decl(ExprInfo* expr_info_ptr, Env* env_ptr) {
  // 添加新的定义。
  if (expr_info_ptr->is_in_decl_stmt() && expr_info_ptr->is_bslog_field_enum_ref()) {
    if (const auto& decl_info = env_ptr->cur_decl_info()) {
      std::vector<std::string> args;

      if (clang::CallExpr* call_expr = dyn_cast<clang::CallExpr>(decl_info->init_expr())) {
        for (size_t i = 0; i < call_expr->getNumArgs(); i++) {
          clang::Expr* arg = call_expr->getArg(i);
          args.emplace_back(stmt_to_string(arg));
        }
      } else if (clang::CXXConstructExpr* cxx_constructor_expr =
                 dyn_cast<clang::CXXConstructExpr>(decl_info->init_expr())) {
        for (size_t i = 0; i < cxx_constructor_expr->getNumArgs(); i++) {
          clang::Expr* arg = cxx_constructor_expr->getArg(i);
          args.emplace_back(stmt_to_string(arg));
        }
      }

      if (args.size() > 0) {
        bool is_bs_field = false;
        bool is_has_value_in_params = false;

        const std::string &var_name = decl_info->name();

        NewVarType new_var_type = NewVarType::SCALAR;
        std::string decl_stmt_str = stmt_to_string(decl_info->decl_stmt());

        if (decl_stmt_str.find("GetSingular") != std::string::npos) {
          new_var_type = NewVarType::SCALAR;
          is_bs_field = true;
        } else if (decl_stmt_str.find("BSRepeatedField") != std::string::npos) {
          new_var_type = NewVarType::LIST;
          is_bs_field = true;
        } else if (decl_stmt_str.find("BSMapField") != std::string::npos) {
          new_var_type = NewVarType::MAP;
          is_bs_field = true;
        }

        if (!is_bs_field) {
          return;
        }

        std::vector<std::string> enum_names;
        std::vector<std::string> enum_var_decl_stmts;

        for (size_t i = 0; i < args.size(); i++) {
          if (args[i] == "*bs" || args[i] == "pos") {
            continue;
          }

          if (absl::StartsWith(args[i], "&")) {
            is_has_value_in_params = true;
            continue;
          }

          if (args[i].find("BSFieldEnum::") != std::string::npos) {
            enum_names.emplace_back(args[i]);
          } else {
            if (clang::Expr *arg_init = env_ptr->find(args[i])) {
              std::string arg_init_str = stmt_to_string(arg_init);
              if (arg_init_str.find("BSFieldEnum::") != std::string::npos) {
                enum_names.emplace_back(args[i]);

                if (clang::DeclStmt *arg_decl_stmt =
                    env_ptr->get_decl_stmt(args[i])) {
                  enum_var_decl_stmts.emplace_back(
                    stmt_to_string(arg_decl_stmt));
                } else {
                  LOG(ERROR) << "cannot find decl_stmt, i: " << i
                             << ", arg: " << args[i];
                }
              }
            } else {
              LOG(ERROR) << "cannot find enum decl in env: " << args[i];
            }
          }
        }

        if (enum_names.size() > 0) {
          if (auto &bs_field_info = env_ptr->touch_bs_field_info(var_name)) {
            bs_field_info->insert_bs_field_enum_var_names(var_name,
                                                          enum_names,
                                                          is_has_value_in_params);
            LOG(INFO) << "insert_bs_field_enum_var_name, var_name: "
                      << var_name
                      << ", enum_names: " << absl::StrJoin(enum_names, ", ")
                      << ", expr: " << expr_info_ptr->origin_expr_str();

            std::ostringstream oss;
            oss << absl::StrJoin(enum_var_decl_stmts, "\n")
                << strip_suffix_semicolon_newline(decl_stmt_str);

            bs_field_info->insert_new_def(var_name, oss.str(),
                                          new_var_type);
          }
        }
      }
    }
  }
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
