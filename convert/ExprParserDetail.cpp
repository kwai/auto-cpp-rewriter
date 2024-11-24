#include <regex>
#include <sstream>
#include <thread>
#include <algorithm>
#include <string>
#include <absl/strings/str_join.h>
#include <absl/strings/str_split.h>

#include "clang/AST/AST.h"

#include "Deleter.h"
#include "ExprParserDetail.h"
#include "Tool.h"
#include "Type.h"
#include "info/IfInfo.h"
#include "info/LoopInfo.h"
#include "info/NewActionParam.h"
#include "info/NewVarDef.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

// =============== 详细逻辑 ===============

// common info 相关逻辑
// auto attr = adlog.item.common_info_attr(i)
// attr.int_value(), attr.int_list_value(), kv.first
// common_info 字段肯定出现在 enum 条件之后，并且一个 if Env 最多只能有一个 common info enum, 其对应的数据
// 只能是一种类型, 或者是单值，或者是 list、或者是 map。
// 1. 如果是单值，则根据 expr_info 则可以确定最重的 bs 表达式，get_bs_field_value 返回 GetSingular 即可。
// 2. 如果是 list, 则此处需要在 Env 中保存变量 list_var_name，get_bs_field_value 返回 list_var_name.Get(i)
// 3. 如果是 map, 则此处需要在 Env 中保存变量 map_var_name, get_bs_field_value 返回 map_var_name.Get(i)
//
// 如果是 list 或者 map, 需要在 Env 中添加其定义, 用于之后生成 bs 表达式。不区分 info 类型，统一放在 Env
// 的 map 中。
// common info 枚举和 repeated common info 的变量肯定不会同时出现。common info 枚举只会在 if 条件中出现,
// 并且只是枚举，不会包含 repeated。而如果 reapted common info 中出现了 common info 的 enum，则肯定是在使用
// common info 的时候，此时 common info 的数据类型就可以根据 callee_name 确定。
//
// common info 第一次出现，只能确定 prefix, 并不能确定是 NORMAL 还是 MULTI_MAP, 因此先将 prefix 记下来,
// 一个 Env 里只能有一个 prefix。
//
// 可能会有两层 loop, 第一层遍历 common info 字段, 第二层是遍历 common info 中的 list 或者 map, 需要找到第一
// loop, 才是 common info prefix 出现的地方。
// 目前通过 is_repeated_common_info 来区分, 如果是 repeated_common_info, 那么就是第一次出现
// prefix 的地方。
//
// 类型也可能在 int_value 之前出现, 如果是这种情况, 那么一定是先遍历 list 或者 map, 再在循环里判断枚举,
// 因此所有的 common_info_method 就都是一样的, 对应的类型也都是一样的, 可以统一在 CommonInfoNormal 中设置。
// 如;
// teams/ad/ad_algorithm/feature/fast/impl/extract_live_lsp_segment_info.h
//
// for (const auto & attr : common_info_attrs) {
//   if (attr.name_value() == attr_name) {
//     for (int64 value : attr.int_list_value()) {
//       if (variant == 0) {
//         AddFeature(value, 1.0f, result);
//         continue;
//       }
//       if (attr_name == auto_cpp_rewriter::CommonInfoAttr_NameExtendTwo_LSP_LATEST_LIVE_SEGMENT_INFO_LIVE_ID) {
//         if (variant == 1) {
//           AddFeature(value & MASK48, 1.0f, result);
//         } else if (variant == 2) {
//           AddFeature(value >> 48, 1.0f, result);
//         }
//         continue;
//       }
//     ...
//     }
//   }
// }
void update_env_common_info_prepare(ExprInfo* expr_info_ptr, Env* env_ptr) {
  update_env_common_info_prefix(expr_info_ptr, env_ptr);
  update_env_common_info_name_value_alias(expr_info_ptr, env_ptr);
  update_env_common_info_method_name(expr_info_ptr, env_ptr);
  update_env_common_info_int_value(expr_info_ptr, env_ptr);
}

void update_env_common_info_prefix(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_repeated_common_info() || expr_info_ptr->is_repeated_common_info_size()) {
    // common info 第一次出现, 设置 common info prefix, 必须是在 for 循环的 range 里。
    // 此时不能确定是 NORMAL 还是 MULTI_MAP, 只能先将 prefix 记录下来。
    if (auto& loop_info = env_ptr->cur_mutable_loop_info()) {
      if (loop_info->loop_stage() == LoopStage::INIT) {
        absl::optional<std::string> prefix_adlog_opt = expr_info_ptr->get_common_info_prefix_adlog();
        if (prefix_adlog_opt) {
          loop_info->set_is_repeated_common_info(true);
          if (Env* loop_env = env_ptr->mutable_loop_env()) {
            const auto& common_info_prefix = loop_env->common_info_prefix();
            if (!common_info_prefix) {
              LOG(INFO) << "set common_info_prefx_adlog: " << *prefix_adlog_opt;
              loop_env->set_common_info_prefix_adlog(*prefix_adlog_opt);
            }
          } else {
            LOG(INFO) << "something is wrong! cannot find loop env, expr: "
                      << stmt_to_string(expr_info_ptr->expr());
          }
        } else {
          LOG(INFO) << "cannot find common_info_prefix_adlog, expr: "
                    << stmt_to_string(expr_info_ptr->expr());
        }
      }
    }
  }
}

void update_env_common_info_name_value_alias(ExprInfo* expr_info_ptr, Env* env_ptr) {
  // 判断等于, 但是不是用 attr.name_value(), 而是模板参数 attr_name。在 CommonInfoNormal 中记录 attr_name。
  // 见: teams/ad/ad_algorithm/feature/fast/impl/extract_live_lsp_segment_info.h
  // for (const auto & attr : common_info_attrs) {
  //   if (attr.name_value() == attr_name) {
  //     for (int64 value : attr.int_list_value()) {
  //     ...
  //     }
  //   }
  // }
  // if (expr_info_ptr->is_common_info_name_value()) {
  //   env_ptr->touch_common_info_normal();
  // }
  if (expr_info_ptr->is_template_int_ref()) {
    if (const auto& if_info = env_ptr->cur_if_info()) {
      if (if_info->if_stage() == IfStage::COND) {
        if (const auto& binary_op_info = env_ptr->cur_binary_op_info()) {
          if (binary_op_info->is_equal_op()) {
            if (binary_op_info->left_expr_str().find("name_value") != std::string::npos) {
              if (auto& common_info_prepare = env_ptr->mutable_common_info_prepare()) {
                if (!common_info_prepare->is_confirmed()) {
                  LOG(INFO) << "set name_value_alias: " << binary_op_info->right_expr_str();
                  common_info_prepare->set_name_value_alias(binary_op_info->right_expr_str());
                }
              }
            }
          }
        }
      }
    }
  }
}

void update_env_common_info_method_name(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (auto& common_info_prepare = env_ptr->mutable_common_info_prepare()) {
    if (!common_info_prepare->is_confirmed()) {
      if (const auto& loop_info = env_ptr->cur_loop_info()) {
        if (loop_info->loop_stage() == LoopStage::INIT) {
          if (expr_info_ptr->is_common_info_list_method()) {
            common_info_prepare->set_method_name(expr_info_ptr->callee_name());
          }
          if (expr_info_ptr->is_common_info_size_method()) {
            common_info_prepare->set_is_for_stmt(loop_info->is_for_stmt());
          }
        }
      }
    }
  }
}

void update_env_common_info_int_value(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_common_attr_info_enum()) {
    if (auto& common_info_prepare = env_ptr->mutable_common_info_prepare()) {
      if (absl::optional<int> enum_value_opt = expr_info_ptr->get_common_attr_int_value()) {
        common_info_prepare->set_int_value(*enum_value_opt);
      }
    }
  }
}

void update_env_common_info_normal_detail_with_value(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_from_repeated_common_info()) {
    if (absl::optional<int> enum_value = env_ptr->get_common_attr_int_value()) {
      if (auto& common_info_normal = env_ptr->mutable_common_info_normal()) {
        if (Env* parent_env_ptr = common_info_normal->parent_env_ptr()) {
          std::string bs_common_enum_str = expr_info_ptr->get_bs_enum_str();
          if (common_info_normal->common_info_details_size() > 0) {
            auto& common_info_detail = common_info_normal->last_mutable_common_info_detail();
            // 是 common info 的取值方法, 如 int_value(), int_list_value()
            if (expr_info_ptr->is_common_info_method()) {
              common_info_detail->update_method_name(expr_info_ptr->callee_name());
            } else if (expr_info_ptr->is_common_info_size_method()) {
              common_info_detail->update_size_method_name(expr_info_ptr->callee_name());
              if (const auto& loop_info = env_ptr->cur_loop_info()) {
                if (loop_info->loop_stage() == LoopStage::INIT) {
                  common_info_detail->set_is_size_method_in_loop_init(true);
                }
              } else {
                common_info_detail->set_is_size_method_in_loop_init(false);
              }
            }
          }
        }
      }
    }
  }
}

void update_env_common_info_normal_detail_without_value(ExprInfo* expr_info_ptr, Env* env_ptr) {
  // 也可能是 CommonInfoFixed, 也找不到 enum_value, 而是模板参数
  if (expr_info_ptr->is_from_repeated_common_info()) {
    if (expr_info_ptr->is_common_info_method()) {
      if (!expr_info_ptr->has_common_attr_int_value_in_env()) {
        if (!env_ptr->is_loop()) {
          if (auto& common_info_fixed_list = env_ptr->touch_common_info_fixed_list()) {
            if (auto last_detail = common_info_fixed_list->last_mutable_common_info_detail()) {
              last_detail->update_method_name(expr_info_ptr->callee_name());
            }
          }
        }

        if (env_ptr->is_loop()) {
          if (const auto& loop_info = env_ptr->cur_loop_info()) {
            if (loop_info->loop_stage() == LoopStage::INIT) {
              // Env 中找不到 int_value, 先出现 common_info_method, 一定是遍历 list 或者 map, 因此可以确定 common_info
              // 对应的类型, 统一在CommonInfoNormal 中设置 uni_method_name
              if (auto& common_info_normal = env_ptr->touch_common_info_normal()) {
                common_info_normal->set_uni_method_name(expr_info_ptr->callee_name());
              }
            }
          }
        }

        if (auto& common_info_multi_map = env_ptr->mutable_common_info_multi_map()) {
          common_info_multi_map->update_method_name(expr_info_ptr->callee_name());
        }
      }
    }
  }
}

void update_env_common_info_normal_name_value(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_from_repeated_common_info()) {
    if (expr_info_ptr->is_common_info_name_value()) {
      if (auto& if_info = env_ptr->mutable_if_info()) {
        if (if_info->if_stage() == IfStage::COND) {
          if_info->add_cond_var_type(ExprType::ADLOG_COMMON_INFO_NAME_VALUE);
        }
      }
    }
  }
}

void update_env_common_info_normal_check_cond_template_int(ExprInfo* expr_info_ptr, Env* env_ptr) {
  // ::auto_cpp_rewriter::ContextInfoCommonAttr::MEDIUM_UID
  if (expr_info_ptr->is_common_attr_info_enum() || expr_info_ptr->is_common_info_compare_int_value(env_ptr)) {
    if (expr_info_ptr->is_template_int_ref()) {
      if (auto& if_info = env_ptr->mutable_if_info()) {
        if (if_info->if_stage() == IfStage::COND) {
          if (if_info->has_cond_var_type(ExprType::ADLOG_COMMON_INFO_NAME_VALUE)) {
            if_info->set_is_check_common_info_normal_cond(true);
          }
        }
      }
    }
  }
}

void update_env_common_info_normal_int_value_in_if(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_common_attr_info_enum() || expr_info_ptr->is_common_info_compare_int_value(env_ptr)) {
    if (!expr_info_ptr->is_template_int_ref()) {
      if (absl::optional<int> enum_value = expr_info_ptr->get_common_attr_int_value_in_expr()) {
        if (auto& if_info = env_ptr->mutable_if_info()) {
          if (if_info->if_stage() == IfStage::COND) {
            if_info->set_is_check_common_info_normal_cond(true);
            absl::optional<int> index;
            if (auto& common_info_normal = env_ptr->touch_common_info_normal()) {
              common_info_normal->add_common_info_value(*enum_value);
              index.emplace(common_info_normal->common_info_details_size() - 1);

              if (auto last_detail = common_info_normal->last_mutable_common_info_detail()) {
                if (absl::optional<std::string> enum_name = expr_info_ptr->get_common_info_enum_name_in_expr()) {
                  last_detail->set_common_info_enum_name(*enum_name);
                }
              }

              if (index) {
                if_info->set_common_info_index(*index);
                if_info->set_common_info_value(*enum_value);
              }

              if (const auto& binary_op_info = env_ptr->cur_binary_op_info()) {
                // 判断不等于
                // teams/ad/ad_algorithm/feature/fast/impl/extract_combine_user_id_dup_cover_id.h:
                // if (!commonAttr.has_name_value()||!commonAttr.has_type()
                //     || commonAttr.type() !=::auto_cpp_rewriter::CommonTypeEnum::INT_ATTR
                //     || commonAttr.name_value() !=
                //     ::auto_cpp_rewriter::ItemCommonInfoAttr_Name::ItemCommonInfoAttr_Name_DSP_DUP_COVER_ID) {
                //   continue;
                // }
                if (if_info->has_cond_var_type(ExprType::ADLOG_COMMON_INFO_NAME_VALUE)) {
                  if (binary_op_info->is_not_equal_op()) {
                    common_info_normal->set_is_check_equal(false);
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

void update_env_common_info_normal_int_value_in_switch(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_common_attr_info_enum() || expr_info_ptr->is_common_info_compare_int_value(env_ptr)) {
    if (!expr_info_ptr->is_template_int_ref()) {
      if (auto& switch_case_info = env_ptr->mutable_switch_case_info()) {
        if (absl::optional<int> enum_value = expr_info_ptr->get_common_attr_int_value_in_expr()) {
          if (auto& common_info_normal = env_ptr->touch_common_info_normal()) {
            absl::optional<int> index;
            common_info_normal->add_common_info_value(*enum_value);
            index.emplace(common_info_normal->common_info_details_size() - 1);

            if (auto last_detail = common_info_normal->last_mutable_common_info_detail()) {
              if (absl::optional<std::string> enum_name = expr_info_ptr->get_common_info_enum_name_in_expr()) {
                last_detail->set_common_info_enum_name(*enum_name);
              }
            }

            LOG(INFO) << "add common info detail, enum_value: " << *enum_value
                      << ", index: " << *index;

            if (index) {
              switch_case_info->set_common_info_index(*index);
              switch_case_info->set_common_info_value(*enum_value);
            }
          }
        } else {
          LOG(INFO) << "cannot get enum value from expr: " << expr_info_ptr->origin_expr_str();
        }
      }
    }
  }
}

void update_env_common_info_normal_enum(ExprInfo* expr_info_ptr, Env* env_ptr) {
  // 先遍历 int_list_value, 再判断 enum
  // teams/ad/ad_algorithm/feature/fast/impl/extract_live_lsp_segment_info.h
  // for (const auto & attr : common_info_attrs) {
  //   if (attr.name_value() == attr_name) {
  //     for (int64 value : attr.int_list_value()) {
  //       if (variant == 0) {
  //         AddFeature(value, 1.0f, result);
  //         continue;
  //       }
  //     }
  //   }
  // }
  if (expr_info_ptr->is_common_attr_info_enum()) {
    // attr 遍历先于枚举
    if (auto& common_info_normal = env_ptr->mutable_common_info_normal()) {
      if (const absl::optional<std::string>& uni_method_name = common_info_normal->uni_method_name()) {
        if (CommonAttrInfo::is_common_info_list_method(*uni_method_name)) {
          if (!common_info_normal->list_loop_var()) {
            if (const Env* loop_env = env_ptr->get_loop_env()) {
              if (loop_env->is_common_info_loop()) {
                if (loop_env->loop_var_names().size() > 0) {
                  LOG(INFO) << "set list_loop_var: " << loop_env->get_last_loop_var();
                  common_info_normal->set_list_loop_var(loop_env->get_last_loop_var());
                }
              }
            }
          }
        }
      }
    }
  }
}

void update_env_common_info_normal_detail_def(ExprInfo* expr_info_ptr, Env* env_ptr) {
  // 必须在添加定义之前执行。
  update_env_common_info_normal_list_loop_var_type(expr_info_ptr, env_ptr);

  // 添加 CommonInfoNormal 定义到 Env
  if (expr_info_ptr->is_common_attr_info_enum() ||
      expr_info_ptr->is_common_info_compare_int_value(env_ptr) ||
      expr_info_ptr->is_common_info_method() ||
      expr_info_ptr->is_common_info_size_method()) {
    if (auto& common_info_normal = env_ptr->mutable_common_info_normal()) {
      if (Env* parent_env_ptr = common_info_normal->parent_env_ptr()) {
        if (common_info_normal->common_info_details_size() > 0) {
          auto& last_detail = common_info_normal->last_mutable_common_info_detail();

          if (expr_info_ptr->is_common_info_method()) {
            last_detail->update_method_name(expr_info_ptr->callee_name());
          }

          if (expr_info_ptr->is_common_info_size_method()) {
            last_detail->update_size_method_name(expr_info_ptr->callee_name());
          }

          LOG(INFO) << "add common info detail def, expr: " << expr_info_ptr->origin_expr_str()
                    << ", is_common_info_size_method: " << expr_info_ptr->is_common_info_size_method()
                    << ", common_info_value: " << last_detail->common_info_value()
                    << ", method_name: " << last_detail->method_name()
                    << ", bs_enum_str: " << last_detail->get_bs_enum_str();
          parent_env_ptr->add_common_info_detail_def(*last_detail);
        }
      }
    }
  }

  // 示例: teams/ad/ad_algorithm/feature/fast/impl/extract_creative_support_tag.h
  // int max_num = attr.string_list_value().size() > 20 ? 20 : attr.string_list_value().size();
  // 示例: teams/ad/ad_algorithm/feature/fast/impl/extract_ad_product_real_name.h
  // attr.string_value().empty()
  if (expr_info_ptr->callee_name() == "size" || expr_info_ptr->callee_name() == "empty") {
    if (auto parent = expr_info_ptr->parent()) {
      if (parent->is_common_info_leaf_method()) {
        if (auto& common_info_normal = env_ptr->mutable_common_info_normal()) {
          if (Env* parent_env_ptr = common_info_normal->parent_env_ptr()) {
            if (auto& common_info_detail = common_info_normal->last_mutable_common_info_detail()) {
              common_info_detail->update_method_name(parent->callee_name());
              LOG(INFO) << "add common info list detail def, expr: " << expr_info_ptr->origin_expr_str();
              parent_env_ptr->add_common_info_detail_def(*common_info_detail);
            }
          }
        }
      }
    }
  }
}

void update_env_common_info_check_list_map(ExprInfo* expr_info_ptr, Env* env_ptr) {
  // for (int64_t value : attr.int_list_value()) { ... }
  bool is_common_info_list = false;
  bool is_common_info_map = false;

  if (expr_info_ptr->is_common_info_list_method()) {
    is_common_info_list = true;
  }

  if (expr_info_ptr->is_common_info_map_method()) {
    is_common_info_map = true;
  }

  if (expr_info_ptr->callee_name() == "" && expr_info_ptr->is_parent_common_info_map_method()) {
    is_common_info_map = true;
  }

  if (is_common_info_list || is_common_info_map) {
    if (auto& loop_info = env_ptr->cur_info(InfoTraits<LoopInfo>::v)) {
      if (loop_info->loop_stage() == LoopStage::INIT) {
        loop_info->set_is_common_info_list_map(true);
        loop_info->set_is_common_info_map(is_common_info_map);
      }
    }
  }
}

void update_env_common_info_normal_size_method(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_common_info_size_method()) {
    if (auto& common_info_normal = env_ptr->mutable_common_info_normal()) {
      if (const auto& loop_info = env_ptr->cur_loop_info()) {
        if (loop_info->loop_stage() == LoopStage::INIT) {
          if (common_info_normal->common_info_details_size() > 0) {
            auto& last_common_info_detail = common_info_normal->last_mutable_common_info_detail();
            last_common_info_detail->set_is_for_stmt(loop_info->is_for_stmt());
          }
        }
      }
    }
  }
}

void update_env_common_info_normal_list_method(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_common_info_list_method()) {
    if (auto& common_info_normal = env_ptr->mutable_common_info_normal()) {
      if (const auto& loop_info = env_ptr->cur_loop_info()) {
        if (loop_info->loop_stage() == LoopStage::INIT) {
          if (common_info_normal->common_info_details_size() > 0) {
            auto& last_common_info_detail = common_info_normal->last_mutable_common_info_detail();
            last_common_info_detail->set_is_for_stmt(loop_info->is_for_stmt());
            last_common_info_detail->set_list_loop_var(loop_info->loop_var());
            last_common_info_detail->update_method_name(expr_info_ptr->callee_name());
          }
        }
      }
    }

    if (auto& common_info_prepare = env_ptr->mutable_common_info_prepare()) {
      if (expr_info_ptr->parent() != nullptr) {
        common_info_prepare->set_attr_name(expr_info_ptr->parent()->origin_expr_str());
      }
    }
  }
}

void update_env_common_info_normal_list_method_address(ExprInfo *expr_info_ptr, Env *env_ptr) {
  if (expr_info_ptr->is_address_expr() &&
      expr_info_ptr->call_expr_params_size() > 0) {
    auto param = expr_info_ptr->call_expr_param(0);
    if (param != nullptr && param->is_common_info_list_method()) {
      if (auto &common_info_normal = env_ptr->mutable_common_info_normal()) {
        if (auto& last_common_info_detail = common_info_normal->last_mutable_common_info_detail()) {
          last_common_info_detail->set_has_list_method_address(true);
        }
      }
    }
  }
}

void update_env_common_info_normal_list_size_method_not_equal(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_binary_op_expr() && expr_info_ptr->callee_name() == "!=") {
    if (auto& if_info = env_ptr->cur_mutable_if_info()) {
      if (if_info->if_stage() == IfStage::COND && if_info->is_body_only_break()) {
        if (expr_info_ptr->call_expr_params_size() == 2) {
          auto param0 = expr_info_ptr->call_expr_param(0);
          auto param1 = expr_info_ptr->call_expr_param(1);
          if (param0 != nullptr && param1 != nullptr) {
            if (param1->is_integral()) {
              if (absl::optional<int> compare_int_value = param1->get_int_value()) {
                LOG(INFO) << "find compare_int_value: " << *compare_int_value;
                if (param0->is_common_info_list_size_method() ||
                    param0->is_common_info_list_size_method_divide_by_int()) {
                  LOG(INFO) << "left is common info list: " << param0->origin_expr_str()
                            << ", set_is_check_common_info_list_size_not_equal: true";
                  if_info->set_is_check_common_info_list_size_not_equal(true);
                  if (auto& common_info_normal = env_ptr->mutable_common_info_normal()) {
                    if (auto& last_detail = common_info_normal->last_mutable_common_info_detail()) {
                      last_detail->set_compare_list_size_vlaue(*compare_int_value);

                      if (param0->is_common_info_list_size_method_divide_by_int()) {
                        if (param0->call_expr_params_size() == 2) {
                          if (auto dividend_info = param0->call_expr_param(1)) {
                            if (absl::optional<int> dividend = dividend_info->get_int_value()) {
                              LOG(INFO) << "last detail set_list_size_dividend: " << *dividend;
                              last_detail->set_list_size_dividend(*dividend);
                            } else {
                              LOG(INFO) << "cannot find dividend from param0: " << param0->origin_expr_str()
                                        << ", binary_operator: " << expr_info_ptr->origin_expr_str();
                            }
                          }
                        }
                      }
                    }
                  }
                }
              } else {
                LOG(INFO) << "cannot find int value from param1: " << param1->origin_expr_str()
                          << ", binary_operator: " << expr_info_ptr->origin_expr_str();
              }
            }
          }
        }
      }
    }
  }
}

void update_env_common_info_normal_helper_method(ExprInfo* expr_info_ptr, Env* env_ptr) {
  // helper 目前只用来处理 common info
  if (expr_info_ptr->is_common_info_list_method() || expr_info_ptr->is_common_info_size_method()) {
    if (const auto& loop_info = env_ptr->cur_loop_info()) {
      if (loop_info->loop_stage() == LoopStage::INIT) {
        if (env_ptr->get_method_name() == "helper") {
          // 更新 feature info 中的 method_info common_info_prepare
          if (auto feature_info = env_ptr->mutable_feature_info()) {
            MethodInfo& method_info = feature_info->touch_method_info("helper");
            auto& common_info_prepare = method_info.mutable_common_info_prepare();
            if (!common_info_prepare) {
              common_info_prepare.emplace();
            }

            if (common_info_prepare) {
              if (expr_info_ptr->is_common_info_list_method()) {
                common_info_prepare->set_method_name(expr_info_ptr->callee_name());
              } else {
                common_info_prepare->update_size_method_name(expr_info_ptr->callee_name());
              }
              if (expr_info_ptr->parent() != nullptr) {
                common_info_prepare->set_attr_name(expr_info_ptr->parent()->origin_expr_str());
              }
            }
          }

          // 更新当前 env_ptr
          auto& prepare = env_ptr->cur_mutable_common_info_prepare();
          if (!prepare) {
            prepare.emplace();
          }

          if (prepare) {
            if (expr_info_ptr->is_common_info_list_method()) {
              prepare->set_method_name(expr_info_ptr->callee_name());
            } else {
              prepare->update_size_method_name(expr_info_ptr->callee_name());
            }
            if (expr_info_ptr->parent() != nullptr) {
              prepare->set_attr_name(expr_info_ptr->parent()->origin_expr_str());
            }
          }
        }
      }
    }
  }
}

void update_env_common_info_normal_helper_def(ExprInfo *expr_info_ptr, Env *env_ptr) {
  // case ::auto_cpp_rewriter:: CommonInfoAttr_NameExtendOne_AD_MERCHANT_FOLLOW_PHOTO_ID_LIST:
  //   helper(FeaturePrefix::USER_AD_MERCHANT_FOLLOW_REAlTIME_NEW_EXTEND_PHOTO, userAttr, result);
  //   break;
  if (expr_info_ptr->is_call_expr() && expr_info_ptr->callee_name() == "helper") {
    if (const auto &switch_case_info = env_ptr->cur_switch_case_info()) {
      if (auto &common_info_normal = env_ptr->mutable_common_info_normal()) {
        if (common_info_normal->common_info_details_size() > 0) {
          if (auto &last_detail = common_info_normal->last_mutable_common_info_detail()) {
            if (auto feature_info = env_ptr->mutable_feature_info()) {
              if (const MethodInfo *method_info = feature_info->find_method_info("helper")) {
                if (const auto &common_info_prepare = method_info->common_info_prepare()) {
                  if (const auto &method_name = common_info_prepare->method_name()) {
                    last_detail->update_method_name(*method_name);
                    if (auto parent_env = env_ptr->mutable_common_info_parent_env()) {
                      LOG(INFO) << "add def, bs_enum_str: "
                                << last_detail->get_bs_enum_str() << ", list_def: "
                                << last_detail->get_bs_list_def(env_ptr);
                      parent_env->add_new_def_meta(last_detail->get_bs_enum_str(),
                                                   last_detail->get_bs_list_def(env_ptr),
                                                   NewVarType::LIST);
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

void update_env_common_info_normal_map_end_cond(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_common_info_map_end()) {
    if (auto& if_info = env_ptr->cur_mutable_if_info()) {
      if (if_info->if_stage() == IfStage::COND) {
        if (const auto& binary_op_info = env_ptr->cur_binary_op_info()) {
          if (binary_op_info->is_not_equal_op()) {
            if_info->set_is_check_common_info_map_end(true);
            if_info->set_is_check_equal(false);
            if_info->set_left_expr_str(binary_op_info->left_expr_str());
          }
        }
      }
    }
  }
}

// const int& pos = *__begin5;
void update_env_common_info_normal_list_loop_var_type(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_common_info_list_method()) {
    if (const auto& loop_info = env_ptr->cur_loop_info()) {
      if (loop_info->loop_stage() == LoopStage::INIT) {
        std::string loop_var_type = loop_info->loop_var_type();
        if (auto &common_info_normal = env_ptr->mutable_common_info_normal()) {
          if (auto last_detail = common_info_normal->last_mutable_common_info_detail()) {
            LOG(INFO) << "set list_loop_var_type: " << loop_var_type;
            last_detail->set_list_loop_var_type(loop_var_type);
          }
        } else if (auto &common_info_fixed_list = env_ptr->mutable_common_info_fixed_list()) {
          if (auto last_detail = common_info_fixed_list->last_mutable_common_info_detail()) {
            LOG(INFO) << "set list_loop_var_type: " << loop_var_type;
            last_detail->set_list_loop_var_type(loop_var_type);
          }
        }
      }
    }
  }
}

// 逻辑比较绕, 之后需要重构一下。
// CommonInfoNormal, CommonInfoMultiMap, CommonInfoFixed 应该是互斥的关系, 需要在 env 中保证。
void update_env_common_info_normal(ExprInfo* expr_info_ptr, Env* env_ptr) {
  // 先出现 common_info_enum, 后出现使用 common info 的变量。
  // 因此出现使用 comon info 的变量时, 必定可以从 Env 中找到对应的 common info enum, 从 method_name
  // 可以判断其数据类型, 是否 list 或者 map。
  // 添加其定义到 env_ptr 中, 如果是单值, 则添加对应的 has 方法到 env_ptr 中, 会在之后的 if 判断中用到。
  // 此时一定已经确定是 NORMAL 还是 MULTI_MAP。
  update_env_common_info_normal_detail_with_value(expr_info_ptr, env_ptr);
  update_env_common_info_normal_detail_without_value(expr_info_ptr, env_ptr);
  update_env_common_info_normal_name_value(expr_info_ptr, env_ptr);

  update_env_common_info_normal_check_cond_template_int(expr_info_ptr, env_ptr);
  update_env_common_info_normal_int_value_in_if(expr_info_ptr, env_ptr);
  update_env_common_info_normal_int_value_in_switch(expr_info_ptr, env_ptr);

  update_env_common_info_normal_enum(expr_info_ptr, env_ptr);
  update_env_common_info_normal_detail_def(expr_info_ptr, env_ptr);
  update_env_common_info_check_list_map(expr_info_ptr, env_ptr);
  update_env_common_info_normal_size_method(expr_info_ptr, env_ptr);
  update_env_common_info_normal_list_method(expr_info_ptr, env_ptr);
  update_env_common_info_normal_list_method_address(expr_info_ptr, env_ptr);
  update_env_common_info_normal_list_size_method_not_equal(expr_info_ptr, env_ptr);
  update_env_common_info_normal_helper_method(expr_info_ptr, env_ptr);
  update_env_common_info_normal_helper_def(expr_info_ptr, env_ptr);
  update_env_common_info_normal_map_end_cond(expr_info_ptr, env_ptr);
}

void update_env_common_info_fixed_list_touch(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_common_info_leaf_method()) {
    if (env_ptr->is_in_common_info_loop_body() || env_ptr->is_in_common_info_if_body()) {
      if (const auto& common_info_prepare = env_ptr->get_common_info_prepare()) {
        if (!common_info_prepare->is_confirmed()) {
          if (common_info_prepare->is_common_info_fixed_list()) {
            env_ptr->touch_common_info_fixed_list();
          }
        }
      }
    }
  }

  if (expr_info_ptr->is_common_info_enum_member_ref()) {
    if (const auto& binary_op_info = env_ptr->get_binary_op_info()) {
      if (ends_with(binary_op_info->left_expr_str(), ".name_value()")) {
        if (const auto& if_info = env_ptr->cur_if_info()) {
          if (if_info->if_stage() == IfStage::COND) {
            if (const auto& common_info_prepare = env_ptr->get_common_info_prepare()) {
              if (!common_info_prepare->is_confirmed()) {
                if (common_info_prepare->is_common_info_fixed_list()) {
                  if (auto& common_info_fixed_list = env_ptr->touch_common_info_fixed_list()) {
                    // pass
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

void update_env_common_info_fixed_list_enum_member_ref(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_common_info_enum_member_ref() || expr_info_ptr->is_template_int_ref()) {
    if (const auto &binary_op_info = env_ptr->get_binary_op_info()) {
      if (ends_with(binary_op_info->left_expr_str(), ".name_value()")) {
        if (auto &if_info = env_ptr->cur_mutable_if_info()) {
          if (if_info->if_stage() == IfStage::COND) {
            if (auto &common_info_fixed_list = env_ptr->touch_common_info_fixed_list()) {
              std::string int_name = tool::trim_this(expr_info_ptr->origin_expr_str());
              common_info_fixed_list->add_int_name(int_name);

              if_info->set_is_check_common_info_fixed_cond(true);
              if_info->set_common_info_int_name(int_name);
              LOG(INFO) << "add_int_name: " << int_name
                        << ", set_is_check_common_info_fixed_cond: true";
            }
          }
        }
      }
    }
  }
}

void update_env_common_info_fixed_list_size_method(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_common_info_size_method()) {
    if (auto& common_info_fixed_list = env_ptr->mutable_common_info_fixed_list()) {
      if (auto last_detail = common_info_fixed_list->last_mutable_common_info_detail()) {
        last_detail->update_size_method_name(expr_info_ptr->callee_name());
      }
    }
  }
}

void update_env_common_info_fixed_list_leaf_method(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_common_info_leaf_method()) {
    if (env_ptr->is_in_common_info_loop_body() || env_ptr->is_in_common_info_if_body()) {
      if (auto& common_info_fixed_list = env_ptr->mutable_common_info_fixed_list()) {
        if (auto last_detail = common_info_fixed_list->last_mutable_common_info_detail()) {
          if (expr_info_ptr->is_common_info_method()) {
            last_detail->update_method_name(expr_info_ptr->callee_name());
          } else {
            last_detail->update_size_method_name(expr_info_ptr->callee_name());
            if (const auto &loop_info = env_ptr->cur_loop_info()) {
              if (loop_info->loop_stage() == LoopStage::INIT) {
                last_detail->set_is_size_method_in_loop_init(true);
                last_detail->set_is_for_stmt(loop_info->is_for_stmt());
              }
            } else {
              last_detail->set_is_size_method_in_loop_init(false);
            }
          }
        }
      }
    }
  }
}

void update_env_common_info_fixed_list_list_method(ExprInfo *expr_info_ptr, Env *env_ptr) {
  // if (user_attr.name_value() == user_attr_name_) {
  //   for (const auto &val : user_attr.int_list_value()) {
  //     action_list.push_back(val);
  //   }
  // }
  if (expr_info_ptr->is_common_info_list_method()) {
    if (env_ptr->is_in_common_info_loop_body() || env_ptr->is_in_common_info_if_body()) {
      if (auto &common_info_fixed_list = env_ptr->mutable_common_info_fixed_list()) {
        if (auto last_detail = common_info_fixed_list->last_mutable_common_info_detail()) {
          last_detail->update_method_name(expr_info_ptr->callee_name());
          if (const auto &loop_info = env_ptr->cur_loop_info()) {
            if (loop_info->loop_stage() == LoopStage::INIT) {
              last_detail->set_is_for_stmt(loop_info->is_for_stmt());
              last_detail->set_list_loop_var(loop_info->loop_var());
              LOG(INFO) << "set fixed list loop_var: " << loop_info->loop_var();
            }
          }
        }
      }
    }
  }
}

void update_env_common_info_fixed_list_list_def(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_common_info_leaf_method()) {
    if (env_ptr->is_in_common_info_loop_body() || env_ptr->is_in_common_info_if_body()) {
      if (auto& common_info_fixed_list = env_ptr->mutable_common_info_fixed_list()) {
        if (auto last_detail = common_info_fixed_list->last_mutable_common_info_detail()) {
          if (auto feature_info = env_ptr->mutable_feature_info()) {
            if (CommonAttrInfo::is_common_info_list_method(expr_info_ptr->callee_name()) ||
                CommonAttrInfo::is_common_info_list_size_method(expr_info_ptr->callee_name())) {
              if (Env *common_info_parent = env_ptr->mutable_common_info_parent_env()) {
                std::string bs_enum_str = last_detail->get_bs_enum_str();
                if (common_info_parent->is_new_var_not_exists(bs_enum_str)) {
                  LOG(INFO) << "add list def, bs_enum_str: " << bs_enum_str
                            << ", def: " << last_detail->get_bs_list_def(env_ptr);
                  common_info_parent->add_new_def(bs_enum_str,
                                                  last_detail->get_bs_list_def(env_ptr),
                                                  NewVarType::LIST);
                  LOG(INFO) << "add list field_def, bs_enum_str: " << bs_enum_str
                            << ", functor_name: " << last_detail->get_functor_name()
                            << ", field_def: " << last_detail->get_bs_list_field_def(env_ptr);
                  feature_info->add_field_def(bs_enum_str,
                                              last_detail->get_functor_name(),
                                              last_detail->get_bs_list_field_def(env_ptr),
                                              NewVarType::LIST,
                                              AdlogVarType::COMMON_INFO_FIXED);
                  feature_info->set_common_info_prefix_name_value(bs_enum_str,
                                                                  last_detail->prefix_adlog(),
                                                                  last_detail->int_name());
                }
              }
            }
          }
        }
      }
    }
  }
}

void update_env_common_info_fixed_list_map_def(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_common_info_leaf_method()) {
    if (env_ptr->is_in_common_info_loop_body() || env_ptr->is_in_common_info_if_body()) {
      if (auto& common_info_fixed_list = env_ptr->mutable_common_info_fixed_list()) {
        if (auto last_detail = common_info_fixed_list->last_mutable_common_info_detail()) {
          if (auto feature_info = env_ptr->mutable_feature_info()) {
            if (CommonAttrInfo::is_common_info_map_method(expr_info_ptr->callee_name())) {
              if (Env *parent = common_info_fixed_list->parent_env_ptr()) {
                parent->add_new_def(last_detail->get_bs_enum_str(),
                                    last_detail->get_bs_map_def(env_ptr),
                                    NewVarType::MAP);
                LOG(INFO) << "add map field_def, bs_enum_str: " << last_detail->get_bs_enum_str()
                          << ", functor_name: "
                          << last_detail->get_functor_name() << ", field_def: "
                          << last_detail->get_bs_map_field_def(env_ptr);
                feature_info->add_field_def(last_detail->get_bs_enum_str(),
                                            last_detail->get_functor_name(),
                                            last_detail->get_bs_map_field_def(env_ptr),
                                            NewVarType::MAP,
                                            AdlogVarType::COMMON_INFO_FIXED);
                feature_info->set_common_info_prefix_name_value(last_detail->get_bs_enum_str(),
                                                                last_detail->prefix_adlog(),
                                                                last_detail->int_name());
              }
            }
          }
        }
      }
    }
  }
}

void update_env_common_info_fixed_list_scalar_def(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_common_info_leaf_method()) {
    if (env_ptr->is_in_common_info_loop_body() || env_ptr->is_in_common_info_if_body()) {
      if (auto& common_info_fixed_list = env_ptr->mutable_common_info_fixed_list()) {
        if (auto last_detail = common_info_fixed_list->last_mutable_common_info_detail()) {
          if (auto feature_info = env_ptr->mutable_feature_info()) {
            if (CommonAttrInfo::is_common_info_scalar_method(expr_info_ptr->callee_name())) {
              if (Env *parent = common_info_fixed_list->parent_env_ptr()) {
                parent->add_new_def(last_detail->get_bs_enum_str(),
                                    last_detail->get_bs_scalar_def(env_ptr),
                                    NewVarType::SCALAR);
                LOG(INFO) << "add scalar field_def, bs_enum_str: " << last_detail->get_bs_enum_str()
                          << ", functor_name: " << last_detail->get_functor_name()
                          << ", field_def: " << last_detail->get_bs_scalar_field_def(env_ptr);
                feature_info->add_field_def(last_detail->get_bs_enum_str(),
                                            last_detail->get_functor_name(),
                                            last_detail->get_bs_scalar_field_def(env_ptr),
                                            last_detail->get_exists_functor_name(),
                                            last_detail->get_bs_scalar_exists_field_def(env_ptr),
                                            NewVarType::SCALAR,
                                            AdlogVarType::COMMON_INFO_FIXED);
                feature_info->set_common_info_prefix_name_value(last_detail->get_bs_enum_str(),
                                                                last_detail->prefix_adlog(),
                                                                last_detail->int_name());
              }
            }
          }
        }
      }
    }
  }
}

void update_env_common_info_fixed_list_list_method_address(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_address_expr() && expr_info_ptr->call_expr_params_size() > 0) {
    auto param = expr_info_ptr->call_expr_param(0);
    if (param != nullptr && param->is_common_info_list_method()) {
      if (auto& common_info_fixed_list = env_ptr->mutable_common_info_fixed_list()) {
        if (auto last_detail = common_info_fixed_list->last_mutable_common_info_detail()) {
          last_detail->set_has_list_method_address(true);
        }

        update_env_common_info_fixed_list_leaf_method(param, env_ptr);
        update_env_common_info_fixed_list_list_method(param, env_ptr);
        update_env_common_info_fixed_list_list_def(param, env_ptr);
      }
    }
  }
}

void update_env_common_info_fixed_list(ExprInfo* expr_info_ptr, Env* env_ptr) {
  // 通过模板参数传递 common info
  // teams/ad/ad_algorithm/feature/fast/impl/extract_ad_ali_feature.h
  // for (const auto &attr : item.ad_dsp_info().photo_info().common_info_attr()) {
  //   if (attr.name_value() == no) {
  //     AddFeature(attr.int_value(), 1.0, result);
  //     break;
  //   }
  // }
  update_env_common_info_fixed_list_touch(expr_info_ptr, env_ptr);
  update_env_common_info_fixed_list_enum_member_ref(expr_info_ptr, env_ptr);
  update_env_common_info_fixed_list_leaf_method(expr_info_ptr, env_ptr);
  update_env_common_info_fixed_list_list_method(expr_info_ptr, env_ptr);
  update_env_common_info_fixed_list_list_def(expr_info_ptr, env_ptr);
  update_env_common_info_fixed_list_map_def(expr_info_ptr, env_ptr);
  update_env_common_info_fixed_list_scalar_def(expr_info_ptr, env_ptr);
  update_env_common_info_fixed_list_size_method(expr_info_ptr, env_ptr);
  update_env_common_info_fixed_list_list_method_address(expr_info_ptr, env_ptr);
}

void update_env_common_info_multi_map_touch(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_find_name_value()) {
    if (auto& loop_info = env_ptr->mutable_loop_info()) {
      env_ptr->add_deleted_var_by_expr(expr_info_ptr->expr());
      env_ptr->touch_common_info_multi_map(expr_info_ptr->get_common_info_multi_map_name(),
                                           expr_info_ptr->get_common_info_multi_attr_name());
      if (auto feature_info = env_ptr->mutable_feature_info()) {
        feature_info->set_has_common_info_multi_map(true);
      }
    }
  }
}

void update_env_common_info_multi_map_check_cond(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_common_info_multi_map_end()) {
    if (auto& if_info = env_ptr->mutable_if_info()) {
      if (if_info->if_stage() == IfStage::COND) {
        if_info->set_is_check_common_info_multi_cond(true);
      }
    }
  }
}

void update_env_common_info_multi_int_list_add_map(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->callee_name() == "operator[]" && expr_info_ptr->call_expr_params_size() == 2) {
    ExprInfo *param0_info_ptr = expr_info_ptr->call_expr_param(0);
    ExprInfo *param1_info_ptr = expr_info_ptr->call_expr_param(1);
    if (param0_info_ptr != nullptr && param1_info_ptr != nullptr) {
      if (param1_info_ptr->callee_name() == "name_value") {
        if (env_ptr->is_in_common_info_loop_body()) {
          if (auto &common_info_multi_int_list = env_ptr->touch_common_info_multi_int_list()) {
            // 确定是 CommonInfoMultiIntList
            if (param0_info_ptr->is_map_repeated_int_list_type()) {
              LOG(INFO) << "add_attr_map_name: " << param0_info_ptr->origin_expr_str();
              common_info_multi_int_list->add_attr_map_name(param0_info_ptr->origin_expr_str());
            }
            if (param0_info_ptr->is_map_int_int_type()) {
              LOG(INFO) << "add_attr_size_map_name: " << param0_info_ptr->origin_expr_str();
              common_info_multi_int_list->add_attr_size_map_name(param0_info_ptr->origin_expr_str());
            }

            if (auto feature_info = env_ptr->mutable_feature_info()) {
              feature_info->set_has_common_info_multi_int_list(true);
            }
          }
        }
      }
    }
  }
}

void update_env_common_info_multi_int_list_def(ExprInfo* expr_info_ptr, Env* env_ptr) {
  // action_name2list[userAttr.name_value()] = &(userAttr.int_list_value());
  if (expr_info_ptr->callee_name() == "operator[]" && expr_info_ptr->call_expr_params_size() == 2) {
    ExprInfo *param0_info_ptr = expr_info_ptr->call_expr_param(0);
    ExprInfo *param1_info_ptr = expr_info_ptr->call_expr_param(1);
    if (param0_info_ptr != nullptr && param1_info_ptr != nullptr) {
      if (param1_info_ptr->callee_name() == "name_value") {
        if (env_ptr->is_in_common_info_loop_body()) {
          // 确定是 CommonInfoMultiIntList
          if (auto &common_info_multi_int_list = env_ptr->touch_common_info_multi_int_list()) {
            if (auto parent_env = common_info_multi_int_list->parent_env_ptr()) {
              if (param0_info_ptr != nullptr && param1_info_ptr != nullptr) {
                std::string map_name = tool::trim_this(param0_info_ptr->origin_expr_str());
                const std::string& vec_name = common_info_multi_int_list->find_correspond_vec_name(map_name);
                if (vec_name.size() > 0) {
                  if (auto feature_info = env_ptr->mutable_feature_info()) {
                    const std::vector<int> &int_values = feature_info->get_int_list_member_values(vec_name);
                    if (param0_info_ptr->is_map_repeated_int_list_type()) {
                      for (int v : int_values) {
                        LOG(INFO) << "add field def, bs_enum_str: "
                                  << common_info_multi_int_list->get_bs_enum_str(v)
                                  << ", bs_list_field_def: "
                                  << common_info_multi_int_list->get_bs_list_field_def(v);
                        feature_info->add_field_def(common_info_multi_int_list->get_bs_enum_str(v),
                                                    common_info_multi_int_list->get_functor_name(v),
                                                    common_info_multi_int_list->get_bs_list_field_def(v),
                                                    NewVarType::LIST,
                                                    AdlogVarType::COMMON_INFO_MULTI_INT_LIST);
                      }
                    }
                  }
                } else {
                  LOG(INFO) << "cannot find correspond vec_name: " << map_name
                            << ", param0: " << param0_info_ptr->origin_expr_str();
                }
              }
            }
          }
        }
      }
    }
  }
}

void update_env_action_detail_prefix(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_action_detail_map()) {
    absl::optional<std::string> action_detail_prefix_adlog = expr_info_ptr->get_action_detail_prefix_adlog();
    if (action_detail_prefix_adlog) {
      env_ptr->add_action_detail_prefix_adlog(*action_detail_prefix_adlog);
    } else {
      LOG(INFO) << "cannot find action_detail_prefix_adlog, expr: " << stmt_to_string(expr_info_ptr->expr());
    }
  }
}

void update_env_action_detail_new_def(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_action_detail_find_expr()) {
    if (!expr_info_ptr->contains_template_parameter()) {
      if(absl::optional<int> action = expr_info_ptr->get_action()) {
        LOG(INFO) << "touch action: " << *action;
        if (auto& action_detail_info = env_ptr->mutable_action_detail_info()) {
          action_detail_info->add_action(*action);
        } else {
          if (auto& action_detail_info = env_ptr->touch_action_detail_info(*action)) {
            // action detail 比较特殊, 以 list_size 是否存在判断是否存在
            // if (absl::optional<std::string> bs_enum_str = action_detail_info->get_bs_list_size_enum_str()) {
            //   action_detail_info->env_ptr()->add_new_exists_def_meta(*bs_enum_str,
            //                                                          action_detail_info->get_action_detail_exists_def(env_ptr));
            // }
          } else {
            LOG(INFO) << "cannot get action_detail_info, expr: " << stmt_to_string(expr_info_ptr->expr());
          }
        }
      } else {
        LOG(INFO) << "something is wrong! cannot find action from: " << expr_info_ptr->to_string();
      }
    }
  }
}

void update_env_action_detail_check_cond(ExprInfo* expr_info_ptr, Env* env_ptr) {
  // if (iter == action_detail.end()) { ... }
  if (expr_info_ptr->is_action_detail_map_end()) {
    if (auto& if_info = env_ptr->mutable_if_info()) {
      if (if_info->if_stage() == IfStage::COND) {
        if_info->set_is_check_action_detail_cond(true);
        if (const auto& action_detail_info = env_ptr->get_action_detail_info()) {
          // action detail 比较特殊, 以 list_size 是否存在判断是否存在。
          // 如果是普通 action_detail, 必须添加到 parent env 中，当前 if 会被整体替换，对应的 env 也会被销毁。
          // 如果是遍历 action_dev_, 则添加到当前 for 循环的 env 中。后面会在 lambda 中用到。
          if (absl::optional<std::string> bs_enum_str = action_detail_info->get_bs_list_size_enum_str()) {
            LOG(INFO) << "add_new_exists_def, bs_enum_str: " << *bs_enum_str
                      << ", def: " << action_detail_info->get_action_detail_exists_def(env_ptr);
            if (const auto& loop_info = env_ptr->get_loop_info()) {
              if (loop_info->is_int_list_member_loop()) {
                env_ptr->add_new_exists_def_helper(*bs_enum_str,
                                                   action_detail_info->get_action_detail_exists_def(env_ptr));
                env_ptr->add_attr_meta(*bs_enum_str);
                return;
              }
            }

            env_ptr->add_new_exists_def_meta(*bs_enum_str,
                                             action_detail_info->get_action_detail_exists_def(env_ptr));
          }
        } else {
          LOG(INFO) << "cannot get action_detail_info, expr: " << stmt_to_string(expr_info_ptr->expr());
        }
      }
    }
  }
}

void update_env_action_detail_action_param_def(ExprInfo* expr_info_ptr, Env* env_ptr) {
  // 函数调用, 添加 action 参数定义
  if (expr_info_ptr->is_cxx_member_call_expr() &&
      env_ptr->is_feature_other_method(expr_info_ptr->get_first_caller_name())) {
    if (const auto feature_info = env_ptr->get_feature_info()) {
      if (const MethodInfo* method_info_ptr =
          feature_info->find_method_info(expr_info_ptr->get_first_caller_name())) {
        for (size_t i = 0; i < expr_info_ptr->call_expr_params_size(); i++) {
          const NewActionParam& new_param = method_info_ptr->find_new_action_param(i);
          if (new_param.origin_name().size() > 0) {
            ExprInfo* call_expr_param = expr_info_ptr->call_expr_param(i);
            if (call_expr_param != nullptr) {
              env_ptr->add_action_param_new_def(call_expr_param->get_bs_enum_str(), new_param);
            } else {
              LOG(INFO) << "cannot find call_expr_param, expr: " << expr_info_ptr->origin_expr_str()
                        << ", index: " << i;
            }
          }
        }
      }
    }
  }
}

void update_env_action_detail_fixed_touch(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_action_detail_find_expr()) {
    if (expr_info_ptr->contains_template_parameter()) {
      if (auto feature_info = env_ptr->mutable_feature_info()) {
        if (absl::optional<std::string> action_name = expr_info_ptr->get_template_action()) {
          if (auto& action_detail_fixed_info = env_ptr->touch_action_detail_fixed_info(*action_name)) {
            LOG(INFO) << "add scalar exists_field_def, bs_enum_str: "
                      << action_detail_fixed_info->get_bs_enum_str("list.size")
                      << ", exists_functor_name: "
                      << action_detail_fixed_info->get_exists_functor_name("list.size")
                      << ", exists_field_def: "
                      << action_detail_fixed_info->get_action_detail_exists_field_def(env_ptr);
            feature_info->add_field_def(action_detail_fixed_info->get_bs_enum_str("list.size"),
                                        action_detail_fixed_info->get_exists_functor_name("list.size"),
                                        action_detail_fixed_info->get_action_detail_exists_field_def(env_ptr),
                                        NewVarType::SCALAR,
                                        ExprType::ACTION_DETAIL_FIXED_HAS,
                                        AdlogVarType::ACTION_DETAIL_FIXED);
            feature_info->set_action_var_name(action_detail_fixed_info->get_bs_enum_str("list.size"), *action_name);

            if (auto constructor_info = env_ptr->mutable_constructor_info()) {
              std::string leaf = action_detail_fixed_info->get_exists_functor_name("list.size");
              constructor_info->add_middle_node_leaf(leaf);
            }
          }
          if (auto feature_info = env_ptr->mutable_feature_info()) {
            // find 的参数可能和模板参数名不一样，需要保存 find 的参数名。
            if (expr_info_ptr->params().size() > 0) {
              feature_info->set_action(stmt_to_string(expr_info_ptr->params()[0]));
            }
          }
        } else {
          LOG(INFO) << "something is wrong! cannot find template action name from: "
                    << expr_info_ptr->to_string();
        }
      }
    }
  }
}

void update_env_action_detail_fixed_var_def(ExprInfo* expr_info_ptr, Env* env_ptr) {
  // 来自 action detail 模板参数的变量
  if (expr_info_ptr->is_from_action_detail_map() &&
      expr_info_ptr->contains_loop_var() &&
      expr_info_ptr->contains_template_parameter() &&
      expr_info_ptr->is_basic()) {
    if (auto feature_info = env_ptr->mutable_feature_info()) {
      if (const auto& action_detail_fixed_info = env_ptr->get_action_detail_fixed_info()) {
        if (absl::optional<std::string> field_name = expr_info_ptr->get_action_detail_field_name()) {
          clang::QualType qual_type = expr_info_ptr->expr()->getType();
          std::string bs_enum_str = action_detail_fixed_info->get_bs_enum_str(*field_name);
          std::string list_def = action_detail_fixed_info->get_bs_list_def(env_ptr, *field_name, qual_type);
          LOG(INFO) << "add list var, bs_enum_str: " << bs_enum_str;
          action_detail_fixed_info->env_ptr()->add_new_def(bs_enum_str,
                                                           list_def,
                                                           NewVarType::LIST);
          LOG(INFO) << "add list field_def, bs_enum_str: " << bs_enum_str
                    << ", functor_name: " << action_detail_fixed_info->get_functor_name(*field_name)
                    << ", field_def: "
                    << action_detail_fixed_info->get_bs_list_field_def(env_ptr, *field_name, qual_type);

          feature_info->add_field_def(bs_enum_str,
                                      action_detail_fixed_info->get_functor_name(*field_name),
                                      action_detail_fixed_info->get_bs_list_field_def(env_ptr,
                                                                                      *field_name,
                                                                                      qual_type),
                                      NewVarType::LIST,
                                      ExprType::ACTION_DETAIL_FIXED_GET,
                                      AdlogVarType::ACTION_DETAIL_FIXED);
          feature_info->set_action_var_name(bs_enum_str, *field_name);
        } else {
          LOG(INFO) << "error, cannot find field_name from: " << expr_info_ptr->to_string();
        }
      }
    }
  }
}

// 中间节点，如 GetPhotoInfo, GetLiveInfo
// auto photo_info = GetPhotoInfo(adlog.item(pos));
// if (photo_info == nullpptr) { }
void update_env_middle_node_root(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_middle_node_root()) {
    if (const auto& decl_info = env_ptr->cur_decl_info()) {
      env_ptr->add_middle_node_name(expr_info_ptr->get_middle_node_root_name());
    }

    // 注册根节点
    if (const auto &middle_node_info = env_ptr->get_middle_node_info()) {
      std::string middle_node_leaf = expr_info_ptr->get_bs_middle_node_leaf();
      if (!expr_info_ptr->is_from_repeated_common_info()) {
        if (auto feature_info = env_ptr->mutable_feature_info()) {
          std::string leaf =
              std::string("BSHas") + expr_info_ptr->get_middle_node_root_name();
          LOG(INFO) << "add leaf: " << leaf;
          LOG(INFO) << "add scalar field_def, bs_enum_str: " << leaf
                    << ", functor_name: " << leaf << ", field_def: "
                    << middle_node_info->get_root_bs_exists_field_def(env_ptr);
          feature_info->add_field_def(leaf,
                                      leaf,
                                      middle_node_info->get_root_bs_exists_field_def(env_ptr),
                                      NewVarType::SCALAR,
                                      AdlogVarType::MIDDLE_NODE_ROOT);
          feature_info->set_middle_node_info(leaf, middle_node_info->name(), "");
        }
      }
    }

    // if 判断 nullptr
    if (auto& if_info = env_ptr->cur_mutable_if_info()) {
      if (if_info->if_stage() == IfStage::COND) {
        if_info->add_cond_var_type(ExprType::ADLOG_MIDDLE_NODE_ROOT);
      }
    }
  }
}

void update_env_middle_node_leaf_def(ExprInfo* expr_info_ptr, Env* env_ptr) {
  LOG(INFO) << "expr: " << expr_info_ptr->origin_expr_str()
            << ", need_replace: " << expr_info_ptr->need_replace()
            << ", is_middle_node_leaf_list_size_method: " << expr_info_ptr->is_middle_node_leaf_list_size_method();
  if (expr_info_ptr->need_replace() && expr_info_ptr->is_from_middle_node()) {
    ExprInfo* new_expr_info_ptr = expr_info_ptr;
    // str 比较特殊，不能用最后的方法, 比如 x.size(), x.data()
    if (expr_info_ptr->parent() != nullptr &&
        expr_info_ptr->is_parent_str_type()) {
      new_expr_info_ptr = expr_info_ptr->parent().get();
    }

    if (auto feature_info = env_ptr->mutable_feature_info()) {
      std::string middle_node_leaf = new_expr_info_ptr->get_bs_middle_node_leaf();
      std::string bs_enum_str = new_expr_info_ptr->get_bs_enum_str();
      if (!new_expr_info_ptr->is_middle_node_root() && !new_expr_info_ptr->is_loop_iter_end()) {
        // TODO(liuzhishan): 需要更精确的判断, 只能添加叶子节点, 中间节点都不能添加, 单值和 list 通过
        // is_basic 可以判断, 但是 map 不行。不过目前中间节点里的数据还没有 map。
        // common info 的统一由 CommonInfoNormal 添加, 这里只处理普通的 middle_node
        if (const auto& middle_node_info = env_ptr->get_middle_node_info()) {
          if (new_expr_info_ptr->is_basic() && !new_expr_info_ptr->is_from_repeated_common_info()) {
            if (middle_node_leaf.find("BSHas") != std::string::npos) {
              std::string exists_field_def =
                middle_node_info->get_bs_exists_field_def(env_ptr,
                                                          middle_node_leaf,
                                                          new_expr_info_ptr->get_middle_node_field());
              LOG(INFO) << "add exists_field_def, bs_enum_str: " << bs_enum_str
                        << ", functor_name: " << middle_node_leaf
                        << ", exists_field_def: " << exists_field_def;

              feature_info->add_field_def(bs_enum_str,
                                          middle_node_leaf,
                                          exists_field_def,
                                          NewVarType::SCALAR,
                                          AdlogVarType::MIDDLE_NODE_LEAF);
              feature_info->set_middle_node_info(bs_enum_str, middle_node_info->name(), new_expr_info_ptr->get_middle_node_field());
            } else if (expr_info_ptr->is_middle_node_leaf_list_size_method()) {
              // leaf list size 方法，需要处理 list。
              // 注意， 需要根据 bs_enum_str 从 feature_info 中获取相应的信息。
              bs_enum_str = expr_info_ptr->get_bs_enum_str_trim_size();
              middle_node_leaf = expr_info_ptr->get_bs_middle_node_leaf_trim_size();
              const auto& middle_node_bs_enum_var_type = feature_info->middle_node_bs_enum_var_type();
              auto it = middle_node_bs_enum_var_type.find(bs_enum_str);
              if (it != middle_node_bs_enum_var_type.end()) {
                if (const auto& list_inner_type = it->second.list_inner_type()) {
                  std::string list_def = middle_node_info->get_bs_list_def(
                    env_ptr, bs_enum_str, middle_node_leaf, *list_inner_type);
                  LOG(INFO) << "add middle node list var def, bs_enum_str: " << bs_enum_str
                            << ", list def : " << list_def
                            << ", inner_type: " << *list_inner_type;
                  env_ptr->add_new_def(bs_enum_str,
                                       list_def,
                                       NewVarType::LIST);

                  std::string list_field_def =
                    middle_node_info->get_bs_list_field_def(env_ptr,
                                                            middle_node_leaf,
                                                            it->second.adlog_field(),
                                                            *list_inner_type);

                  LOG(INFO) << "add middle node field def, bs_enum_str: " << bs_enum_str
                            << ", list field def : " << list_field_def
                            << ", adlog_field: " << it->second.adlog_field()
                            << ", inner_type: " << *list_inner_type;
                  feature_info->add_field_def(bs_enum_str,
                                              middle_node_leaf,
                                              list_field_def,
                                              NewVarType::LIST,
                                              AdlogVarType::MIDDLE_NODE_LEAF);
                  feature_info->set_middle_node_info(bs_enum_str, middle_node_info->name(), it->second.adlog_field());
                } else {
                  LOG(INFO) << "cannot find middle node list inner type in feature_info"
                            << ", bs_enum_str: " << bs_enum_str
                            << ", expr: " << new_expr_info_ptr->origin_expr_str();
                }
              }
            } else {
              // 目前还只有 list 和 scalar，暂时不考虑 map
              LOG(INFO) << "expr: " << new_expr_info_ptr->origin_expr_str()
                        << ", type_str: " << new_expr_info_ptr->expr()->getType().getAsString()
                        << ", is_repeated_proto_type: " << new_expr_info_ptr->is_repeated_proto_type();
              if (new_expr_info_ptr->is_repeated_proto_iterator_type() ||
                  new_expr_info_ptr->is_repeated_proto_type() ||
                  new_expr_info_ptr->is_repeated_proto_ptr()) {
                if (absl::optional<std::string> inner_type =
                    tool::get_repeated_proto_inner_type(new_expr_info_ptr->expr()->getType())) {
                  std::string list_def = middle_node_info->get_bs_list_def(env_ptr,
                                                                           bs_enum_str,
                                                                           middle_node_leaf,
                                                                           *inner_type);
                  env_ptr->add_new_def(bs_enum_str,
                                       list_def,
                                       NewVarType::LIST);

                  std::string list_field_def =
                    middle_node_info->get_bs_list_field_def(env_ptr,
                                                            middle_node_leaf,
                                                            new_expr_info_ptr->get_middle_node_field(),
                                                            *inner_type);

                  LOG(INFO) << "add list field_def, bs_enum_str: " << bs_enum_str
                            << ", expr: " << new_expr_info_ptr->origin_expr_str()
                            << ", functor_name: " << middle_node_leaf
                            << ", list_field_def: " << list_field_def
                            << ", inner_type: " << *inner_type;
                  feature_info->add_field_def(bs_enum_str,
                                              middle_node_leaf,
                                              list_field_def,
                                              NewVarType::LIST,
                                              AdlogVarType::MIDDLE_NODE_LEAF);
                  feature_info->set_middle_node_info(bs_enum_str,
                                                     middle_node_info->name(),
                                                     new_expr_info_ptr->get_middle_node_field());
                } else {
                  LOG(INFO) << "cannot find inner_type from type_str: " << new_expr_info_ptr->expr()->getType().getAsString()
                            << ", expr: " << new_expr_info_ptr->to_string();
                }

                if (auto& loop_info = env_ptr->cur_mutable_loop_info()) {
                  if (loop_info->loop_stage() == LoopStage::INIT) {
                    loop_info->set_is_middle_node_proto_list_loop(true);
                    loop_info->set_loop_var_expr_bs_enum_str(new_expr_info_ptr->get_bs_enum_str());
                  }
                }
              } else {
                // scalar
                std::string field_def =
                  middle_node_info->get_bs_scalar_field_def(env_ptr,
                                                            middle_node_leaf,
                                                            new_expr_info_ptr->get_middle_node_field(),
                                                            new_expr_info_ptr->expr()->getType());

                LOG(INFO) << "add field_def, bs_enum_str: " << bs_enum_str
                          << ", expr: " << new_expr_info_ptr->origin_expr_str()
                          << ", functor_name: " << middle_node_leaf
                          << ", field_def: " << field_def
                          << ", type: " << new_expr_info_ptr->expr()->getType().getAsString();
                feature_info->add_field_def(bs_enum_str,
                                            middle_node_leaf,
                                            field_def,
                                            NewVarType::SCALAR,
                                            AdlogVarType::MIDDLE_NODE_LEAF);
                feature_info->set_middle_node_info(bs_enum_str,
                                                   middle_node_info->name(),
                                                   new_expr_info_ptr->get_middle_node_field());

                // middle node str 比较特殊，可能会访问其 data() 和 size() 方法，因此需要单独添加变量定义。
                if (new_expr_info_ptr->is_string()) {
                  std::string str_scalar_def =
                    middle_node_info->get_bs_str_scalar_def(env_ptr,
                                                            bs_enum_str,
                                                            middle_node_leaf);
                  LOG(INFO) << "add middle node str scalar def, bs_enum_str: "<< bs_enum_str
                            << ", scalar def: " << str_scalar_def;
                  env_ptr->add_new_def(bs_enum_str, str_scalar_def, NewVarType::SCALAR);
                }
              }
            }
          }
        }
      }
    }
  }
}

// if (photo_info == nullpptr) { }
void update_env_middle_node_check_cond(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_nullptr()) {
    if (auto& if_info = env_ptr->cur_mutable_if_info()) {
      if (if_info->if_stage() == IfStage::COND) {
        if (if_info->has_cond_var_type(ExprType::ADLOG_MIDDLE_NODE_ROOT)) {
          // 必定是出现在 if (photo_info == nullptr) 的判断中
          LOG(INFO) << "set_is_check_middle_node_root_cond: true, expr: "
                    << expr_info_ptr->origin_expr_str();
          if_info->set_is_check_middle_node_root_cond(true);
        }
      }
    }
  }
}

void update_env_double_list(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_from_adlog() && !expr_info_ptr->is_from_repeated_common_info()) {
    if (expr_info_ptr->is_var_proto_list()) {
      if (auto& loop_info = env_ptr->cur_mutable_loop_info()) {
        if (loop_info->loop_stage() == LoopStage::INIT) {
          loop_info->set_is_proto_list_loop(true);
          if (env_ptr->parent() != nullptr) {
            if (auto& parent_loop_info = env_ptr->parent()->cur_mutable_loop_info()) {
              parent_loop_info->set_is_child_proto_list_loop(true);
            }
          }
        }
      }
    }
  }
}

void update_env_get_seq_list_touch(ExprInfo *expr_info_ptr, Env *env_ptr) {
  if (expr_info_ptr->is_seq_list_root() && !expr_info_ptr->is_seq_list_reco_proto_type()) {
    if (const auto& decl_info = env_ptr->cur_decl_info()) {
      const auto& seq_list_info = env_ptr->get_seq_list_info();
      if (!seq_list_info) {
        env_ptr->touch_seq_list_info(decl_info->name());
      }
    }
  }
}

void update_env_get_seq_list_def(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_seq_list_root_deref() && !expr_info_ptr->is_seq_list_reco_proto_type()) {
    if (const auto& decl_info = env_ptr->cur_decl_info()) {
      if (decl_info->name().size() > 0) {
        if (expr_info_ptr->call_expr_params_size() > 0) {
          auto param_expr_info_ptr = expr_info_ptr->call_expr_param(0);
          if (param_expr_info_ptr != nullptr) {
            std::string caller_name = param_expr_info_ptr->get_first_caller_name();
            std::string type_str = param_expr_info_ptr->expr()->getType().getAsString();
            if (auto &seq_list_info = env_ptr->mutable_seq_list_info()) {
              seq_list_info->update(decl_info->name(), caller_name, type_str);
              env_ptr->add_new_def(decl_info->name(),
                                   seq_list_info->get_def(),
                                   seq_list_info->get_var_type());
            }
          }
        }
      }
    }
  }
}

void update_env_get_seq_list_if_cond(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_seq_list_root_ref() && !expr_info_ptr->is_seq_list_reco_proto_type()) {
    if (auto& if_info = env_ptr->cur_mutable_if_info()) {
      if (auto& binary_op_info = env_ptr->cur_mutable_binary_op_info()) {
        if (binary_op_info->is_not_equal_op()) {
          if (binary_op_info->right_expr_str() == "nullptr") {
            if_info->set_is_check_seq_list_cond(true);
          }
        }
      }
    }
  }
}

void update_env_get_seq_list_loop(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_from_seq_list()) {
    if (auto& loop_info = env_ptr->cur_mutable_loop_info()) {
      if (loop_info->loop_stage() == LoopStage::INIT) {
        loop_info->set_is_seq_list_loop(true);
      }
    }
  }
}

void update_env_proto_list_leaf(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_general_adlog_var()) {
    if (auto& loop_info = env_ptr->mutable_loop_info()) {
      if (loop_info->is_proto_list_loop() &&
          env_ptr->is_in_loop_body() &&
          loop_info->prefix_adlog().size() > 0) {
        if (const auto parent = loop_info->parent_env_ptr()) {
          const auto& parent_loop_info = parent->get_loop_info();
          // 确定是单层 proto list loop
          if (!parent_loop_info) {
            std::string prefix_adlog = loop_info->prefix_adlog();
            if (auto& proto_list_info = loop_info->mutable_env_ptr()->touch_proto_list_info(prefix_adlog)) {
              LOG(INFO) << "touch_proto_list_info: " << prefix_adlog
                        << ", prefix: " << proto_list_info->prefix()
                        << ", prefix_adlog: " << proto_list_info->prefix_adlog()
                        << ", expr: " << expr_info_ptr->origin_expr_str();

              std::string adlog_str = expr_info_ptr->get_adlog_field_str();
              if (prefix_adlog.size() > 0 && adlog_str.size() > prefix_adlog.size()) {
                // 不包括叶子节点，field_str 长度必须大于 0
                if (absl::optional<std::string> field_str =
                        expr_info_ptr->get_adlog_field_str_after_loop_var()) {
                  if (*field_str != "size") {
                    proto_list_info->add_field(*field_str);
                    LOG(INFO) << "add proto_list field: " << *field_str;
                  }
                } else {
                  LOG(INFO) << "cannot find field after loop_var: " << expr_info_ptr->origin_expr_str()
                            << ", loop_var: " << loop_info->loop_var();
                }
              } else {
                LOG(INFO) << "loop_var prefix_adlog is empty, loop_var: "
                          << loop_info->loop_var();
              }
            }
          } else {
            LOG(INFO) << "find parent loop of loop!";
          }
        } else {
          LOG(INFO) << "parent of loop is nullptr!";
        }
      }
    }
  }
}

void update_env_proto_list_size(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_general_proto_list_size_method()) {
  }
}

// iter->second or it->second
void update_env_general_iter_second(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_iter_second()) {
    if (auto& loop_info = env_ptr->mutable_loop_info()) {
      loop_info->set_loop_iter(expr_info_ptr->get_first_decl_ref());
    }
  }
}

void update_env_general_basic_scalar_def(ExprInfo* expr_info_ptr, Env* env_ptr) {
  // 比较特殊，在 general_loop_var_method 中处理。
  if (expr_info_ptr->is_loop_var_size_method()) {
    return;
  }

  // 在 general_str_call 中单独处理
  if (expr_info_ptr->is_parent_str_ref() || expr_info_ptr->is_parent_str_type()) {
    return;
  }

  if (expr_info_ptr->is_general_proto_list_size_method()) {
    return;
  }

  if (expr_info_ptr->is_method_is_train()) {
    return;
  }

  if (expr_info_ptr->is_from_query_token() || expr_info_ptr->is_from_photo_text()) {
    return;
  }

  LOG(INFO) << "expr: " << expr_info_ptr->to_string()
            << ", is_from_reco_user_info: " << expr_info_ptr->is_from_reco_user_info();

  // 单值, 不包括 list size
  if (expr_info_ptr->is_basic_scalar() &&
      expr_info_ptr->is_from_adlog() &&
      !expr_info_ptr->is_list_size_method() &&
      !expr_info_ptr->is_from_reco_user_info() &&
      !expr_info_ptr->is_from_seq_list() &&
      !expr_info_ptr->is_from_repeated_common_info()) {
    std::string bs_enum_str = expr_info_ptr->get_bs_enum_str();
    Env* target_env = env_ptr;
    // 比较特殊，可以定义在 root env。
    if (bs_enum_str == "adlog_time") {
      target_env = env_ptr->get_mutable_root();
    }

    if (target_env == nullptr) {
      LOG(INFO) << "get root env failed!";
      return;
    }
    // 单值, 如 int64_t user_id = adlog.user_info().id()
    //
    // 一个变量只应该被定义一次, 如果别别的变量引用, 则别的变量不替换, 例
    //   int64_t user_id = adlog.user_info().id();
    //   int64_t user_id_v1 = user_id;
    // 被替换为
    //   int64_t user_id = BSFieldHelper::GetSingular<int64_t>(*bs, BSFieldEnum::adlog_user_info_id);
    //   int64_t user_id_v1 = user_id;
    if (!expr_info_ptr->is_decl_ref_expr()) {
      if (env_ptr->is_new_var_not_exists(bs_enum_str)) {
        if (const auto &decl_info = env_ptr->cur_decl_info()) {
          if (!starts_with(decl_info->name(), "__") &&
              !starts_with(decl_info->name(), "* __")) {
            if (expr_info_ptr->to_string() ==
                stmt_to_string(decl_info->init_expr())) {
              LOG(INFO)
                << "add basic sclar in decl, bs_enum_str: " << bs_enum_str
                << ", var_name: " << decl_info->name()
                << ", expr: " << expr_info_ptr->origin_expr_str()
                << ", def: "
                << expr_info_ptr->get_bs_scalar_def(decl_info->name());
              target_env->add_new_def_meta(
                bs_enum_str, decl_info->name(),
                expr_info_ptr->get_bs_scalar_def(decl_info->name()),
                NewVarType::SCALAR);
              target_env->set_normal_adlog_field_info(bs_enum_str, expr_info_ptr->get_adlog_field_str());
            } else {
              LOG(INFO) << "add basic sclar, bs_enum_str: " << bs_enum_str
                        << ", expr: " << expr_info_ptr->origin_expr_str()
                        << ", def: " << expr_info_ptr->get_bs_scalar_def();
              target_env->add_new_def_meta(
                bs_enum_str, expr_info_ptr->get_bs_scalar_def(),
                NewVarType::SCALAR);
              target_env->set_normal_adlog_field_info(bs_enum_str, expr_info_ptr->get_adlog_field_str());
            }
          }
        } else {
          LOG(INFO) << "add basic sclar, bs_enum_str: " << bs_enum_str
                    << ", expr: " << expr_info_ptr->origin_expr_str()
                    << ", def: " << expr_info_ptr->get_bs_scalar_def()
                    << ", rewrite_reco_user_info: " << GlobalConfig::Instance()->rewrite_reco_user_info;
          target_env->add_new_def_meta(bs_enum_str,
                                    expr_info_ptr->get_bs_scalar_def(),
                                    NewVarType::SCALAR);
          target_env->set_normal_adlog_field_info(bs_enum_str, expr_info_ptr->get_adlog_field_str());
        }
      }
    }
  }
}

void update_env_general_str_call(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_from_adlog() &&
      expr_info_ptr->is_cxx_member_call_expr() &&
      !expr_info_ptr->is_from_query_token() &&
      !expr_info_ptr->is_from_photo_text() &&
      !expr_info_ptr->is_from_middle_node() &&
      !expr_info_ptr->is_from_repeated_common_info() &&
      !expr_info_ptr->is_from_implicit_loop_var()) {
    // string.data()
    if (expr_info_ptr->is_parent_str_ref() || expr_info_ptr->is_parent_str_type()) {
      if (auto expr_parent = expr_info_ptr->parent()) {
        std::string expr_str = expr_parent->origin_expr_str();
        std::string bs_enum_str = expr_parent->get_bs_enum_str();
        if (tool::is_adlog_field(bs_enum_str)) {
          if (env_ptr->is_new_var_not_exists(bs_enum_str)) {
            if (env_ptr->is_loop_var(expr_str) || expr_parent->is_from_list()) {
              // 如果存在会忽略
              LOG(INFO) << "add loop var str list def, bs_enum_str: " << bs_enum_str
                        << ", list def: " << expr_parent->get_bs_list_def();
              env_ptr->add_new_def_meta(bs_enum_str, expr_parent->get_bs_list_def(), NewVarType::LIST);
              env_ptr->set_normal_adlog_field_info(bs_enum_str, expr_parent->get_adlog_field_str());
            } else {
              LOG(INFO) << "add scalar var str def, bs_enum_str: " << bs_enum_str
                        << ", scalar def: " << expr_parent->get_bs_scalar_def();
              env_ptr->add_new_def_meta(bs_enum_str, expr_parent->get_bs_scalar_def(), NewVarType::SCALAR);
              env_ptr->set_normal_adlog_field_info(bs_enum_str, expr_parent->get_adlog_field_str());
            }
          }
        }
      }
    }
  }
}

void update_env_general_loop_var_method(ExprInfo *expr_info_ptr, Env *env_ptr) {
  // xxx.size() or xxx.data()
  if (expr_info_ptr->is_from_adlog() &&
      expr_info_ptr->is_parent_loop_var_ref() &&
      !expr_info_ptr->is_from_query_token() &&
      !expr_info_ptr->is_from_photo_text() &&
      expr_info_ptr->is_cxx_member_call_expr() &&
      (expr_info_ptr->callee_name() == "size" || expr_info_ptr->callee_name() == "data") &&
      !expr_info_ptr->is_from_repeated_common_info() &&
      !expr_info_ptr->is_from_implicit_loop_var()) {
    // seg.size()
    if (auto expr_parent = expr_info_ptr->parent()) {
      std::string expr_str = expr_parent->origin_expr_str();
      std::string bs_enum_str = expr_parent->get_bs_enum_str();
      // 如果存在会忽略
      if (starts_with(bs_enum_str, "adlog") || starts_with(bs_enum_str, "ad_log")) {
        if (auto feature_info = env_ptr->get_feature_info()) {
          if (feature_info->is_in_bs_enum_var_type(bs_enum_str)) {
            if (env_ptr->is_new_var_not_exists(bs_enum_str)) {
              LOG(INFO) << "add loop var list def, bs_enum_str: " << bs_enum_str
                        << ", list def: " << expr_parent->get_bs_list_def();
              env_ptr->add_new_def_meta(bs_enum_str,
                                        expr_parent->get_bs_list_def(),
                                        NewVarType::LIST);
              env_ptr->set_normal_adlog_field_info(bs_enum_str, expr_parent->get_adlog_field_str());
            }
          }
        }
      }
    }
  }
}

// 自定义的 adlog 变量, 需要在 Env 中添加定义。
// 注意: 与 basic_scalar 有区别, 可能是来自 list 的变量，最终的取值是普通类型。
//
// 此处只处理非 common info 的情况, 并且不包含模板参数。
//
// 1. list
//   expr 在循环中，并且其中包含循环变量，则必是来自 list。如
//   for(int i = 0; i < follow_num && i < max_follow_num; ++i){
//   const auto & follow_info = action_detail.follow(i);
//   if(follow_info.id() == 90041) {
//     continue;
//   }
//
// 2. 单值
//    如 adlog.user_info().id()
//
// 和上面的 scalar 逻辑有重复，需要整理下。is_basic 可能来自 list, is_basic_scalar 必须来自单值。
// LOG(INFO) << "expr: " << expr_info_ptr->origin_expr_str()
//           << ", is_from_adlog: " << expr_info_ptr->is_from_adlog()
//           << ", is_basic: " << expr_info_ptr->is_basic()
//           << ", is_from_middle_node: " << expr_info_ptr->is_from_middle_node()
//           << ", is_from_action_detail: " << expr_info_ptr->is_from_action_detail_map()
//           << ", is_decl_ref_expr: " << expr_info_ptr->is_decl_ref_expr()
//           << ", contains_template_parameter(): " << expr_info_ptr->contains_template_parameter();
void update_env_general_basic_expr(ExprInfo* expr_info_ptr, Env* env_ptr) {
  LOG(INFO) << "expr: " << expr_info_ptr->origin_expr_str()
            << ", is general adlog var: " << expr_info_ptr->is_general_adlog_var()
            << ", is_from_adlog: " << expr_info_ptr->is_from_adlog()
            << ", is_from_reco_user_info: " << expr_info_ptr->is_from_reco_user_info()
            << ", is_from_implicit_loop_var: " << expr_info_ptr->is_from_implicit_loop_var()
            << ", is_decl_ref_expr: " << expr_info_ptr->is_decl_ref_expr()
            << ", contains_loop_var: " << expr_info_ptr->contains_loop_var()
            << ", is_basic: " << expr_info_ptr->is_basic()
            << ", is_basic_scalar: " << expr_info_ptr->is_basic_scalar()
            << ", is_from_list: " << expr_info_ptr->is_from_list()
            << ", is_from_map: " << expr_info_ptr->is_from_map()
            << ", expr_info_ptr->is_from_repeated_common_info(): " << expr_info_ptr->is_from_repeated_common_info()
            << ", is_cxx_operator_call_expr: " << expr_info_ptr->is_cxx_operator_call_expr()
            << ", is_cxx_operator_call_expr_deref: " << expr_info_ptr->is_cxx_operator_call_expr_deref()
            << ", is_loop_var_size_method: " << expr_info_ptr->is_loop_var_size_method()
            << ", is_general_proto_list_size_method: " << expr_info_ptr->is_general_proto_list_size_method()
            << ", is_from_query_token: " << expr_info_ptr->is_from_query_token()
            << ", is_from_photo_text: " << expr_info_ptr->is_from_photo_text();
  if (expr_info_ptr->is_loop_var_size_method()) {
    return;
  }

  // 在 general_str_call 中单独处理
  if (expr_info_ptr->is_parent_str_ref() || expr_info_ptr->is_parent_str_type()) {
    return;
  }

  // 叶子节点 size 不需要添加定义。
  if (expr_info_ptr->is_general_proto_list_size_method()) {
    return;
  }

  // reco user info list size
  if (expr_info_ptr->is_list_size_method()) {
    return;
  }

  // 在 QueryToken 中处理。
  if (expr_info_ptr->is_from_query_token() || expr_info_ptr->is_from_photo_text()) {
    return;
  }

  if (expr_info_ptr->is_general_adlog_var()) {
    std::string bs_enum_str = expr_info_ptr->get_bs_enum_str();
    if (!env_ptr->is_in_for_range_init() && env_ptr->is_new_var_not_exists(bs_enum_str)) {
      if (!expr_info_ptr->is_from_repeated_common_info() &&
          (expr_info_ptr->contains_loop_var() || expr_info_ptr->is_from_list())) {
        // list
        // 逻辑类似，可以合并
        if (const auto& loop_info = env_ptr->get_loop_info()) {
          LOG(INFO) << "add list var def with meta, in loop body, bs_enum_str: " << bs_enum_str
                    << ", def: " << expr_info_ptr->get_bs_list_def()
                    << ", expr: " << expr_info_ptr->origin_expr_str();
          env_ptr->add_new_def_meta(bs_enum_str, expr_info_ptr->get_bs_list_def(), NewVarType::LIST);
          env_ptr->set_normal_adlog_field_info(bs_enum_str, expr_info_ptr->get_adlog_field_str());
          if (auto& loop_info = env_ptr->mutable_loop_info()) {
            std::string var_name = env_ptr->find_new_var_name(bs_enum_str);
            if (var_name.size() > 0) {
              loop_info->add_leaf_field(var_name);
            } else {
              LOG(ERROR) << "cannot find var_name, bs_enum_str: " << bs_enum_str
                         << ", expr: " << expr_info_ptr->to_string();
            }
          }
        } else if (expr_info_ptr->is_action_detail_leaf()) {
          // 来自 action detail, 可能是按固定下标获取，没有 for 循环。
          // 示例: teams/ad/ad_algorithm/feature/fast/impl/extract_live_user_impression_avegap_new.h
          // const auto &ad_live_action_detail = adlog.user_info().ad_live_action_detail();
          // auto iter = ad_live_action_detail.find(no);
          // if (iter == ad_live_action_detail.end()) {
          //   return;
          // }
          // const auto &live_infos = iter->second.list();
          // if (live_infos.size() < 2) {
          //   return;
          // }
          // if ((live_infos[0].action_time() < live_infos[live_infos.size() - 1].action_time())) {
          //   return;
          // }

          LOG(INFO) << "add list var def with meta, bs_enum_str: " << bs_enum_str
                    << ", def: " << expr_info_ptr->get_bs_list_def()
                    << ", expr: " << expr_info_ptr->origin_expr_str();
          env_ptr->add_new_def_meta(bs_enum_str, expr_info_ptr->get_bs_list_def(), NewVarType::LIST);
          env_ptr->set_normal_adlog_field_info(bs_enum_str, expr_info_ptr->get_adlog_field_str());
        } else if (absl::optional<std::string> int_param = expr_info_ptr->find_int_param()) {
          LOG(INFO) << "add list var def with meta, has int_param, bs_enum_str: " << bs_enum_str
                    << ", def: " << expr_info_ptr->get_bs_list_def()
                    << ", expr: " << expr_info_ptr->origin_expr_str();
          env_ptr->add_new_def_meta(bs_enum_str, expr_info_ptr->get_bs_list_def(), NewVarType::LIST);
          env_ptr->set_normal_adlog_field_info(bs_enum_str, expr_info_ptr->get_adlog_field_str());
        } else {
          LOG(INFO) << "cannot find loop parent, expr: " << stmt_to_string(expr_info_ptr->expr());
        }
      } else if (!expr_info_ptr->is_decl_ref_expr() && expr_info_ptr->is_basic_scalar()) {
        // 单值, 如 int64_t user_id = adlog.user_info().id()
        //
        // 一个变量只应该被定义一次, 如果别别的变量引用, 则别的变量不替换, 例
        //   int64_t user_id = adlog.user_info().id();
        //   int64_t user_id_v1 = user_id;
        // 被替换为
        //   int64_t user_id = BSFieldHelper::GetSingular<int64_t>(*bs, BSFieldEnum::adlog_user_info_id);
        //   int64_t user_id_v1 = user_id;
          if (const auto &decl_info = env_ptr->cur_decl_info()) {
            if (!starts_with(decl_info->name(), "__") && !starts_with(decl_info->name(), "* __")) {
              if (expr_info_ptr->to_string() == stmt_to_string(decl_info->init_expr())) {
                LOG(INFO) << "add scalar def from decl, bs_enum_str: " << bs_enum_str
                          << ", name: " << decl_info->name()
                          << ", scalar def: " << expr_info_ptr->get_bs_scalar_def(decl_info->name());
                env_ptr->add_new_def_meta(bs_enum_str, decl_info->name(),
                                             expr_info_ptr->get_bs_scalar_def(decl_info->name()),
                                             NewVarType::SCALAR);
                env_ptr->set_normal_adlog_field_info(bs_enum_str, expr_info_ptr->get_adlog_field_str());
              } else {
                LOG(INFO) << "add scalar def, bs_enum_str: " << bs_enum_str
                          << ", scalar def: " << expr_info_ptr->get_bs_scalar_def();
                env_ptr->add_new_def_meta(bs_enum_str,
                                             expr_info_ptr->get_bs_scalar_def(),
                                             NewVarType::SCALAR);
                env_ptr->set_normal_adlog_field_info(bs_enum_str, expr_info_ptr->get_adlog_field_str());
              }
            }
          } else {
            LOG(INFO) << "add scalar def, bs_enum_str: " << bs_enum_str
                      << ", def: " << expr_info_ptr->get_bs_scalar_def();
            env_ptr->add_new_def_meta(bs_enum_str,
                                         expr_info_ptr->get_bs_scalar_def(),
                                         NewVarType::SCALAR);
            env_ptr->set_normal_adlog_field_info(bs_enum_str, expr_info_ptr->get_adlog_field_str());
          }
        }
    }
  }
}

void update_env_general_binary_op_info(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (auto& binary_op_info = env_ptr->cur_info(InfoTraits<BinaryOpInfo>::v)) {
    if (binary_op_info->is_assign_op()) {
      // 当前 stmt 已经赋值，必须查看父级 Env
      if (Env* parent = env_ptr->parent()) {
        if (parent->is_template_int_ref(expr_info_ptr->origin_expr())) {
          if (expr_info_ptr->origin_expr_str() == binary_op_info->left_expr_str()) {
            binary_op_info->set_left_expr_type(ExprType::TEMPLATE_INT_REF);
          }
        }
      }

      if (is_integer(expr_info_ptr->origin_expr_str())) {
        if (expr_info_ptr->origin_expr_str() == binary_op_info->right_expr_str()) {
          binary_op_info->set_right_expr_type(ExprType::INT);
        }
      }
    }
  }
}

void update_env_general_decl_info(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (const auto& decl_info = env_ptr->cur_decl_info()) {
    // 来自 adlog 的变量都需要删除, 在 Env 中重新添加定义。
    LOG(INFO) << "expr: " << stmt_to_string(expr_info_ptr->expr())
              << ", origin_expr: " << expr_info_ptr->raw_expr_str()
              << ", is_from_adlog: " << expr_info_ptr->is_from_adlog()
              << ", is_from_list: " << expr_info_ptr->is_from_list()
              << ", is_reco_proto: " << expr_info_ptr->is_reco_proto_type()
              << ", is_from_seq_list: " << expr_info_ptr->is_from_seq_list()
              << ", is_from_seq_list_reco: " << expr_info_ptr->is_from_seq_list_reco()
              << ", need_delete: " << Deleter::need_delete(expr_info_ptr, env_ptr);
    if (Deleter::need_delete(expr_info_ptr, env_ptr)) {
      env_ptr->add_deleted_var_by_expr_str(expr_info_ptr->raw_expr_str());
    }
  }
}

void update_env_general_get_norm_query(ExprInfo* expr_info_ptr, Env* env_ptr) {
  // 在 ConstructorInfo 中标记是否用到 GetNormQuery
  if (expr_info_ptr->is_get_norm_query()) {
    if (auto constructor_info = env_ptr->mutable_constructor_info()) {
      constructor_info->set_has_get_norm_query(true);
    }
  }
}

void update_env_general_loop_var_expr(ExprInfo* expr_info_ptr, Env* env_ptr) {
  // 添加 for range loop 变量到 loop_info
  if (auto& loop_info = env_ptr->cur_mutable_loop_info()) {
    if (const auto& decl_info = env_ptr->cur_decl_info()) {
      if (loop_info->loop_stage() == LoopStage::INIT) {
        if (tool::is_implicit_loop_var(decl_info->name())) {
          loop_info->set_loop_var_expr(expr_info_ptr->get_loop_var_expr());
          loop_info->set_prefix_adlog(expr_info_ptr->get_adlog_field_str());
          LOG(INFO) << "set loop_var_expr: " << stmt_to_string(expr_info_ptr->get_loop_var_expr())
                    << ", set prefix_adlog: " << expr_info_ptr->get_adlog_field_str();
        }
      }
    }
  }
}

// 示例: teams/ad/ad_algorithm/feature/fast/impl/extract_match_dense_num.h,
//    action_vec_ 是 std::vector<int> 类型, 构造函数中会进行初始化。
//
// const auto& ad_action = adlog.user_info().explore_long_term_ad_action();
// for (auto action_no : action_vec_) {
//   auto action_no_iter = ad_action.find(action_no);
//   ...
// }
//
// 先根据第一个 int 替换, 然后用字符串替换生成多个 lambda 函数。
void update_env_general_int_list_member_loop(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_cxx_member_call_expr() && expr_info_ptr->callee_name() == "end") {
    if (expr_info_ptr->is_from_implicit_loop_var() && expr_info_ptr->parent() != nullptr) {
      LOG(INFO) << "expr: " << expr_info_ptr->origin_expr_str()
                << ", is_int_list_member_ref: "
                << expr_info_ptr->parent()->is_int_list_member_ref();
      if (expr_info_ptr->parent()->is_int_list_member_ref()) {
        if (auto &loop_info = env_ptr->cur_mutable_loop_info()) {
          if (!loop_info->is_for_stmt() &&
              loop_info->loop_stage() == LoopStage::INIT) {
            loop_info->set_is_int_list_member_loop(true);
            if (const auto feature_info = env_ptr->get_feature_info()) {
              std::string loop_var_expr_str = tool::trim_this(loop_info->loop_var_expr_str());
              std::vector<int> values = feature_info->get_int_list_member_values(loop_var_expr_str);
              loop_info->set_int_list_member_values(values);
              loop_info->set_int_list_index(0);
              LOG(INFO) << "set_int_list_member_values: " << absl::StrJoin(values, ",")
                        << ", loop_var_expr_str: " << loop_var_expr_str
                        << ", int_list_index: " << 0;
            }
          }
        }
      }
    }
  }

  // 来自自定义变量的 int list
  // 示例: teams/ad/ad_algorithm/feature/fast/impl/extract_combine_user_click_match_num_long.h
  // std::vector<int> urb_type_array = {1, 2, 3, 4, 5, 6, 8, 19};
  // for (int i = 0; i < urb_type_array.size(); i++) {
  //   int urb_type = urb_type_array[i];
  //   const auto& ad_action = adlog.user_info().explore_long_term_ad_action();
  //   auto iter = ad_action.find(urb_type);
  //   if (iter == ad_action.end()) {
  //     ...
  //   } else {
  //     ...
  //   }
  // }
  if (expr_info_ptr->is_cxx_member_call_expr() &&
      expr_info_ptr->callee_name() == "size" &&
      expr_info_ptr->parent() != nullptr) {
    if (expr_info_ptr->parent()->is_int_list_var_ref()) {
      if (auto& loop_info = env_ptr->cur_mutable_loop_info()) {
        if (loop_info->is_for_stmt() && loop_info->loop_stage() == LoopStage::INIT) {
          loop_info->set_is_int_list_member_loop(true);

          std::string loop_var_expr_str = tool::trim_this(expr_info_ptr->parent()->origin_expr_str());
          clang::Expr* init_expr = env_ptr->find(loop_var_expr_str);
          if (init_expr != nullptr) {
            std::vector<int> values = tool::get_int_list_values_from_init_str(stmt_to_string(init_expr));
            loop_info->set_int_list_member_values(values);
            loop_info->set_int_list_index(0);
            LOG(INFO) << "set_int_list_member_values: " << absl::StrJoin(values, ",")
                      << ", loop_var_expr_str: " << loop_var_expr_str << ", int_list_index: " << 0;
          }
        }
      }
    }
  }
}

// 检查 item_size, 但是所有逻辑都在 if body 里，并不是提前 return。在 if_info 中标记。
// if (adlog.item_size() > pos) {
//   ...
// }
// 或者
// if (pos < adlog.item_size()) {
// ...
// }
// 或者
// if (adlog.item_size() <= pos) {
//   return;
// }
// 或者
// if (pos >= adlog.item_size()) {
//   return;
// }
void update_env_general_check_item_pos_include(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_pos_ref()) {
    if (auto& if_info = env_ptr->mutable_if_info()) {
      if (if_info->if_stage() == IfStage::COND) {
        if (const auto& binary_op_info = env_ptr->cur_binary_op_info()) {
          if (expr_info_ptr->origin_expr_str() == binary_op_info->right_expr_str() &&
              (binary_op_info->is_greater_op() || binary_op_info->is_greater_equal_op()) &&
              binary_op_info->left_expr_str() == "adlog.item_size()") {
            // if (adlog.item_size() >= pos) { ... }
            if_info->set_is_check_item_pos_include_cond(true);
          } else if (binary_op_info->left_expr_str() == "adlog.item_size()" &&
                     (binary_op_info->is_less_equal_op() || binary_op_info->is_less_op())) {
            // if (adlog.item_size() <= pos) { return; }
            if_info->set_is_check_item_pos_include_cond(false);
          } else if (binary_op_info->left_expr_str() == "pos" &&
                   (binary_op_info->is_less_op() || binary_op_info->is_less_equal_op()) &&
                   binary_op_info->right_expr_str() == "adlog.item_size()") {
            // if (pos < adlog.item_size()) { ... }
            if_info->set_is_check_item_pos_include_cond(true);
          } else if (binary_op_info->left_expr_str() == "pos" &&
                     (binary_op_info->is_greater_op() || binary_op_info->is_greater_equal_op()) &&
                     binary_op_info->right_expr_str() == "adlog.item_size()") {
            // if (pos >= adlog.item_size()) { return; }
            if_info->set_is_check_item_pos_include_cond(false);
          }
        }
      }
    }
  }
}

void update_env_general_reco_user_info(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr == nullptr || env_ptr == nullptr) {
    return;
  }

  if (expr_info_ptr->is_from_adlog()) {
    std::string bs_enum_str = expr_info_ptr->get_bs_enum_str();
    if (tool::is_str_from_reco_user_info(bs_enum_str)) {
      if (env_ptr->is_in_loop_init()) {
        if (auto& loop_info = env_ptr->cur_mutable_loop_info()) {
          if (GlobalConfig::Instance()->rewrite_reco_user_info) {
            loop_info->set_is_reco_user_info_loop(true);
          } else {
            loop_info->set_is_reco_user_info_loop(false);
          }
        }
      }
    }
  }
}

void update_env_general_proto_map_loop(ExprInfo* expr_info_ptr, Env* env_ptr) {
  if (expr_info_ptr->is_var_proto_map() &&
      !expr_info_ptr->is_from_repeated_common_info() &&
      !expr_info_ptr->is_from_query_token() &&
      !expr_info_ptr->is_from_photo_text() &&
      !expr_info_ptr->is_from_middle_node()) {
    if (!starts_with(expr_info_ptr->origin_expr_str(), "__") &&
        !starts_with(expr_info_ptr->origin_expr_str(), "*")) {
      // 正常的变量，不是隐式变量
      if (auto &loop_info = env_ptr->cur_mutable_loop_info()) {
        if (loop_info->loop_stage() == LoopStage::INIT) {
          loop_info->set_is_general_proto_map_loop(true);
          std::string bs_enum_str = expr_info_ptr->get_bs_enum_str();
          if (auto parent = env_ptr->parent()) {
            if (parent->is_new_var_not_exists(bs_enum_str)) {
              LOG(INFO) << "add new var map, bs_enum_str: " << bs_enum_str
                        << ", expr: " << expr_info_ptr->origin_expr_str()
                        << ", map_def: " << expr_info_ptr->get_bs_map_def();
              parent->add_new_def(bs_enum_str, expr_info_ptr->get_bs_map_def(),
                                  NewVarType::MAP);
              parent->add_attr_meta(bs_enum_str + "_key");
              parent->add_attr_meta(bs_enum_str + "_value");
            }
          }
        }
      }
    }
  }
}

void update_env_general_proto_list_size_method(ExprInfo* expr_info_ptr, Env* env_ptr) {
  // 叶子节点 proto list
  // end 来自循环变量
  if (!expr_info_ptr->is_from_repeated_common_info()) {
    if (expr_info_ptr->callee_name() == "size" || expr_info_ptr->is_from_implicit_loop_var()) {
      if (auto parent = expr_info_ptr->parent().get()) {
        if (parent->is_repeated_proto_list_leaf_type()) {
          std::string bs_enum_str = parent->get_bs_enum_str();
          if (bs_enum_str.size() > 0 && tool::is_adlog_field(bs_enum_str)) {
            LOG(INFO) << "add proto list leaf def, bs_enum_str: " << bs_enum_str
                      << ", list def: " << parent->get_bs_list_def();
            env_ptr->add_new_def_meta(bs_enum_str, parent->get_bs_list_def(), NewVarType::LIST);
            env_ptr->set_normal_adlog_field_info(bs_enum_str, parent->get_adlog_field_str());
          }
        }
      }
    }
  }
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
