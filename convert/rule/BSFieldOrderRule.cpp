#include <absl/strings/match.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <vector>
#include <string>
#include "../Env.h"
#include "../Tool.h"
#include "../ExprParser.h"
#include "../info/NewVarDef.h"
#include "./BSFieldOrderRule.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

using nlohmann::json;
using tool::replace_simple_text;
using tool::insert_str_at_block_begin;

json BSFieldOrderRule::process_to_json(clang::DeclStmt *decl_stmt, Env *env_ptr) {
  return json::array();
}

json BSFieldOrderRule::process_to_json(clang::DeclRefExpr* decl_ref_expr, Env* env_ptr) {
  json res = json::array();

  auto expr_info_ptr = parse_expr(decl_ref_expr, env_ptr);

  if (const auto &decl_info = env_ptr->cur_decl_info()) {
    if (expr_info_ptr->origin_expr_str() == decl_info->name()) {
      return res;
    }
  }

  std::string bs_var_name = expr_info_ptr->origin_expr_str();
  // 先找到 decl_stmt, 再找到 enum 的 decl_stmt, 删掉 decl_stmt。
  // 然后将新的定义写入到正确的 env 中。
  //
  // 可能有如下形式
  //   if (has_val) { ... }
  //
  // 这种情况需要判断 if env 的 parent。如果 if cond 表达式在 bs field 定义中出现过，则是这种情况。
  if (expr_info_ptr->is_bslog_field_var_decl()) {
    if (env_ptr->is_in_loop_init() || env_ptr->is_in_loop_body() || env_ptr->is_in_if_cond()) {
      if (env_ptr->is_parent_loop()) {
        // 可能有两层 for 循环，需要定义在最外层 for 循环的 parent 中。
        if (auto* outer_env_ptr = env_ptr->mutable_outer_loop_parent()) {
          if (!outer_env_ptr->is_decl_in_cur_env(bs_var_name)) {
            fix_bs_field_decl(bs_var_name, env_ptr, outer_env_ptr);
          } else {
            set_bs_field_visited(bs_var_name, env_ptr);
          }
        } else {
          LOG(ERROR) << "cannot find outer loop parent!";
        }
      } else if (env_ptr->is_parent_if()) {
        if (env_ptr->is_in_parent_else()) {
          if (auto* outer_parent_ptr = env_ptr->mutable_outer_if_parent()) {
            if (!outer_parent_ptr->is_decl_in_cur_env(bs_var_name)) {
              fix_bs_field_decl(bs_var_name, env_ptr, outer_parent_ptr);
            } else {
              set_bs_field_visited(bs_var_name, env_ptr);
            }
          }
        } else {
          if (!env_ptr->is_decl_in_parent_env(bs_var_name)) {
            fix_bs_field_decl(bs_var_name, env_ptr, env_ptr->parent());
          } else {
            set_bs_field_visited(bs_var_name, env_ptr);
          }
        }
      } else {
        if (!env_ptr->is_decl_in_parent_env(bs_var_name)) {
          fix_bs_field_decl(bs_var_name, env_ptr, env_ptr->parent());
        } else {
          set_bs_field_visited(bs_var_name, env_ptr);
        }
      }
    } else if (env_ptr->is_in_if_body()) {
      if (env_ptr->is_parent_if()) {
        if (env_ptr->is_in_parent_else()) {
          if (auto *outer_parent_ptr = env_ptr->mutable_outer_if_parent()) {
            if (!outer_parent_ptr->is_decl_in_cur_env(bs_var_name)) {
              fix_bs_field_decl(bs_var_name, env_ptr, outer_parent_ptr);
            } else {
              set_bs_field_visited(bs_var_name, env_ptr);
            }

            return json::array();
          }
        }
      }

      // 如果 bs field 定义中有 has_value，则必定用在 if cond 中判断是否有值，因此需要判断 parent env。
      if (is_has_value_in_bs_field_params(bs_var_name, env_ptr)) {
        if (!env_ptr->is_decl_in_parent_env(bs_var_name)) {
          fix_bs_field_decl(bs_var_name, env_ptr, env_ptr->parent());
        } else {
          set_bs_field_visited(bs_var_name, env_ptr);
        }
      } else if (!env_ptr->is_decl_in_cur_env(bs_var_name)) {
        fix_bs_field_decl(bs_var_name, env_ptr, env_ptr);
      } else {
        set_bs_field_visited(bs_var_name, env_ptr);
      }
    } else {
      if (env_ptr->is_parent_if()) {
        if (env_ptr->is_in_parent_else()) {
          if (auto *outer_parent_ptr = env_ptr->mutable_outer_if_parent()) {
            if (!outer_parent_ptr->is_decl_in_cur_env(bs_var_name)) {
              fix_bs_field_decl(bs_var_name, env_ptr, outer_parent_ptr);
            } else {
              set_bs_field_visited(bs_var_name, env_ptr);
            }

            return json::array();
          }
        }
      } else {
        set_bs_field_visited(bs_var_name, env_ptr);
      }
    }
  }

  return res;
}

void BSFieldOrderRule::fix_bs_field_decl(const std::string& bs_var_name, Env* env_ptr, Env* target_env_ptr) {
  if (auto* bs_field_detail_ptr = env_ptr->find_bs_field_detail_ptr_by_var_name(bs_var_name)) {
    auto it = bs_field_detail_ptr->find(bs_var_name);
    if (it != bs_field_detail_ptr->end()) {
      if (it->second.is_visited == false) {
        const auto &field_enum_names = it->second.enum_var_names;

        if (field_enum_names.size() > 0) {
          if (clang::Stmt *decl_stmt = env_ptr->get_decl_stmt(bs_var_name)) {
            rewriter_.ReplaceText(decl_stmt, "");
          }

          for (size_t i = 0; i < field_enum_names.size(); i++) {
            if (!absl::StartsWith(field_enum_names[i], "BSFieldEnum::")) {
              // 可能是枚举，也可能是变量，只有变量才需要删除。
              if (clang::Stmt *field_enum_decl_stmt =
                      env_ptr->get_decl_stmt(field_enum_names[i])) {
                rewriter_.ReplaceText(field_enum_decl_stmt, "");
              }
            }
          }

          if (target_env_ptr != nullptr) {
            LOG(INFO) << "add new def bs: " << bs_var_name
                      << ", is target env if: " << target_env_ptr->is_if();
            target_env_ptr->add_new_def_overwrite(
                bs_var_name, it->second.new_def, it->second.new_var_type);
            it->second.is_visited = true;
          }
        } else {
          LOG(ERROR) << "field_enum_names.size() is 0, bs_var_name: " << bs_var_name;
        }
      }
    } else {
      LOG(ERROR) << "cannot find def in bs_field_info, bs_var_name: " << bs_var_name;
    }
  } else {
    LOG(ERROR) << "cannot find bs_field_info, bs_var_name: " << bs_var_name;
  }
}

json BSFieldOrderRule::process_to_json(clang::IfStmt *if_stmt, Env *env_ptr) {
  std::ostringstream oss;

  std::string if_text = rewriter_.getRewrittenText(if_stmt);

  std::string new_defs_str = env_ptr->get_all_new_defs();
  if (new_defs_str.size() > 0) {
    std::string s = insert_str_at_block_begin(if_text, new_defs_str);
    rewriter_.ReplaceText(if_stmt, replace_simple_text(s));
  }

  return json::array();
}

json BSFieldOrderRule::process_to_json(clang::ForStmt *for_stmt, Env *env_ptr) {
  return json::array();
}

json BSFieldOrderRule::process_to_json(clang::CXXForRangeStmt *cxx_for_range_stmt, Env *env_ptr) {
  return json::array();
}

json BSFieldOrderRule::process_to_json(clang::CXXMemberCallExpr* cxx_member_call_expr, Env* env_ptr) {
  if (cxx_member_call_expr == nullptr || env_ptr == nullptr) {
    return json::array();
  }

  auto expr_info_ptr = parse_expr(cxx_member_call_expr, env_ptr);
  if (expr_info_ptr == nullptr) {
    LOG(ERROR) << "parse expr failed! expr: " << stmt_to_string(cxx_member_call_expr);
    return json::array();
  }

  clang::Expr* caller = cxx_member_call_expr->getImplicitObjectArgument();
  auto caller_expr_info_ptr = parse_expr(caller, env_ptr);

  if (caller_expr_info_ptr == nullptr) {
    LOG(ERROR) << "parse expr failed! expr: " << stmt_to_string(caller);
    return json::array();
  }

  std::string bs_var_name = caller_expr_info_ptr->origin_expr_str();
  bool contains_loop_var = expr_info_ptr->contains_loop_var();

  // 先找到 decl_stmt, 再找到 enum 的 decl_stmt, 删掉 decl_stmt。
  // 然后将新的定义写入到正确的 env 中。
  if (env_ptr->is_bslog_field_var_decl(bs_var_name)) {
    if (env_ptr->is_in_loop_init() || env_ptr->is_in_loop_body() || env_ptr->is_in_if_cond()) {
      if (contains_loop_var) {
        // 如果包含循环变量，则必须找到最外层的 loop
        if (auto *outer_env_ptr = env_ptr->mutable_outer_loop_parent()) {
          if (!outer_env_ptr->is_decl_in_cur_env(bs_var_name)) {
            fix_bs_field_decl(bs_var_name, env_ptr, outer_env_ptr);
          }
        } else {
          LOG(ERROR) << "cannot find outer loop parent! bs_var_name: " << bs_var_name;
        }
      } else {
        if (!env_ptr->is_parent_loop() && !env_ptr->is_decl_in_parent_env(bs_var_name)) {
          fix_bs_field_decl(bs_var_name, env_ptr, env_ptr->parent());
        } else if (env_ptr->is_parent_loop()) {
          if (auto *outer_env_ptr = env_ptr->mutable_outer_loop_parent()) {
            if (!outer_env_ptr->is_decl_in_cur_env(bs_var_name)) {
              fix_bs_field_decl(bs_var_name, env_ptr, outer_env_ptr);
            } else {
              set_bs_field_visited(bs_var_name, env_ptr);
            }
          }
        } else {
          set_bs_field_visited(bs_var_name, env_ptr);
        }
      }
    } else if (env_ptr->is_in_if_body()) {
      if (contains_loop_var) {
        if (auto *outer_env_ptr = env_ptr->mutable_outer_loop_parent()) {
          if (!outer_env_ptr->is_decl_in_cur_env(bs_var_name)) {
            fix_bs_field_decl(bs_var_name, env_ptr, outer_env_ptr);
          } else {
            set_bs_field_visited(bs_var_name, env_ptr);
          }
        } else {
          LOG(ERROR) << "cannot find outer loop parent! bs_var_name: "
                     << bs_var_name;
        }
      } else {
        if (env_ptr->is_parent_if()) {
          if (env_ptr->is_in_parent_else()) {
            if (auto *outer_parent_ptr = env_ptr->mutable_outer_if_parent()) {
              if (is_has_value_in_bs_field_params(bs_var_name, env_ptr)) {
                // 单值需要判断这种情况，list 和 map 不用。
                if (!outer_parent_ptr->is_decl_in_cur_env(bs_var_name)) {
                  fix_bs_field_decl(bs_var_name, env_ptr, outer_parent_ptr);
                } else {
                  set_bs_field_visited(bs_var_name, env_ptr);
                }
              } else if (!outer_parent_ptr->is_decl_in_cur_env(bs_var_name)) {
                fix_bs_field_decl(bs_var_name, env_ptr, outer_parent_ptr);
              } else {
                set_bs_field_visited(bs_var_name, env_ptr);
              }
            } else {
              set_bs_field_visited(bs_var_name, env_ptr);
            }
          }
        } else {
          if (is_has_value_in_bs_field_params(bs_var_name, env_ptr)) {
            // 单值需要判断这种情况，list 和 map 不用。
            if (!env_ptr->is_decl_in_parent_env(bs_var_name)) {
              fix_bs_field_decl(bs_var_name, env_ptr, env_ptr->parent());
            } else {
              set_bs_field_visited(bs_var_name, env_ptr);
            }
          } else if (!env_ptr->is_decl_in_cur_env(bs_var_name)) {
            fix_bs_field_decl(bs_var_name, env_ptr, env_ptr);
          } else {
            set_bs_field_visited(bs_var_name, env_ptr);
          }
        }
      }
    }
  }

  return json::array();
}

json BSFieldOrderRule::process_to_json(clang::UnaryOperator *unary_operator, Env *env_ptr) {
  auto expr_info_ptr = parse_expr(unary_operator, env_ptr);

  return json::array();
}

void BSFieldOrderRule::set_bs_field_visited(const std::string& bs_var_name, Env* env_ptr) {
  if (auto *bs_field_detail_ptr = env_ptr->find_bs_field_detail_ptr_by_var_name(bs_var_name)) {
    auto it = bs_field_detail_ptr->find(bs_var_name);
    if (it != bs_field_detail_ptr->end()) {
      it->second.is_visited = true;
    } else {
      LOG(ERROR) << "cannot find def in bs_field_info, bs_var_name: "
                 << bs_var_name;
    }
  } else {
    LOG(ERROR) << "cannot find bs_field_info, bs_var_name: " << bs_var_name;
  }
}

bool BSFieldOrderRule::is_has_value_in_bs_field_params(const std::string& bs_var_name, Env* env_ptr) const {
  if (env_ptr == nullptr) {
    LOG(ERROR) << "env_ptr is nullptr!";
    return false;
  }

  if (auto *bs_field_detail_ptr = env_ptr->find_bs_field_detail_ptr_by_var_name(bs_var_name)) {
    auto it = bs_field_detail_ptr->find(bs_var_name);
    if (it != bs_field_detail_ptr->end()) {
      if (it->second.is_has_value_in_params) {
        return true;
      } else {
        return false;
      }
    } else {
      LOG(ERROR) << "cannot find def in bs_field_info, bs_var_name: "
                 << bs_var_name;
      return false;
    }
  } else {
    LOG(ERROR) << "cannot find bs_field_info, bs_var_name: " << bs_var_name;
    return false;
  }
}

// ad_algorithm/bs_feature/fast/impl/bs_extract_ad_first_industry_v3_dense_onehot_v1.cc

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
