#include <absl/types/optional.h>
#include <string>
#include <sstream>
#include <regex>
#include <cctype>
#include <iostream>

#include <absl/strings/str_join.h>
#include <absl/strings/str_split.h>
#include <clang/AST/Stmt.h>

#include "./Config.h"
#include "./Env.h"
#include "./Tool.h"
#include "./ExprInfo.h"
#include "./info/CommonInfo.h"
#include "./handler/StrictRewriter.h"
#include "./info/LoopInfo.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

std::unordered_set<std::string> ExprInfo::ignore_callee_ = {
  "int_value",
  "float_value",
  "string_value",
  "int_list_value",
  "string_list_value",
  "map_int64_int64_value",
  "map_int64_float_value",
  "map_int64_string_value",
  "first",
  "find",
  "at",
  "second",
  "operator[]",
  "operator->",
  "Get"
};

bool ExprInfo::need_replace() const {
  // product_name.find
  if (is_from_adlog() && is_caller_str_ref()) {
    return true;
  }

  if (is_from_reco_user_info_real()) {
    if (GlobalConfig::Instance()->rewrite_reco_user_info) {
      return true;
    } else {
      return false;
    }
  }

  if (is_from_adlog()) {
    return true;
  }

  if (parent_ != nullptr && CommonAttrInfo::is_common_info_leaf_method(callee_name_)) {
    return true;
  }

  return false;
}

// 需要改写的变量最终肯定是单值的变量，类型是基础类型，需要区分普通表达式和中间节点，以及自定义变量,
// 有以下几种情况:
//
// 1. 普通表达式, 主要是普通 adlog 字段以及 common _info 的字段, 所使用的 bs 字段都可以在编译器确定
//   a. 本身就是单值，返回 BSFieldHelper::GetSingular<T>(*bs, enum_str, pos)。
//   b. 来自 list, 返回 list_var_name.Get(i), list_var_name 是从 Env 中获取的 list 变量，i 是循环变量。
//   c. 来自 map, kv.first 表示 key, 返回 map_var_name.GetKey(i), kv.second 表示 value, 返回
//      map_var_name.GetValue(i), map_var_name 是从 Env 中获取的 map 变量。
//
// 2. 中间节点, 主要是来自 PhotoInfo, LiveInfo 等中间节点的字段, 所使用的的 bs 字段在编译器确定不了，需要在
//    运行期根据 item_type 等字段的取值决定用哪个 bs 字段, 这些节点的变量统一提供了模板函数来处理, 直接将
//    变量替换为对应的 bs_util 函数即可, 如 bs_util.GetPhotoInfoXXX(), list 和 map 也是类似的处理。
//
// 3. 自定义变量, 里面可能包含 common info, 如 std::vector<CommonAttrInfo>。
//    最后访问的时候还是会按照 CommonAttrInfo 访问, 如:
//      action_list[key_idx].int_list_value_size();
//      action_list[key_idx].int_list_value(i)
//    这种情况 is_from_adlog() 一定是 false, 因此不需要替换为 bs 的表单式，只需要将最后的方法名修改即可, 如:
//      action_list[key_idx].size();
//      action_list[key_idx].Get(i);
std::string ExprInfo::get_bs_field_value() const {
  if (is_from_adlog()) {
    if (is_from_reco_user_info()) {
      return get_bs_field_value_reco_user_info();
    } else if (is_from_query_token() || is_from_photo_text()) {
      return get_bs_field_value_query_token();
    } else if (is_common_info_map_iter_second()) {
      if (parent_ != nullptr && !parent_->is_loop_var_ref()) {
        return parent_->origin_expr_str() + ".first";
      } else {
        return get_bs_field_value_normal();
      }
    } else if (is_loop_var_size_method()) {
      return get_bs_field_value_loop_var_size();
    } else if (is_general_proto_list_size_method()) {
      return get_bs_field_value_general_proto_list_size_method();
    } else if (is_from_middle_node()) {
      if (is_from_repeated_common_info()) {
        return get_bs_field_value_normal();
      } else if (is_middle_node_leaf_list_size_method()) {
        return get_bs_field_value_middle_node_leaf_list_size_method();
      } else {
        return get_bs_field_value_middle_node();
      }
    } else if (is_action_detail_list_size_expr()) {
      return get_bs_field_value_action_detail_list_size();
    } else if (is_from_seq_list() || is_from_seq_list_reco()) {
      return origin_expr_str();
    } else {
      return get_bs_field_value_normal();
    }
  } else if (parent_ != nullptr && CommonAttrInfo::is_common_info_leaf_method(callee_name_)) {
    if (CommonAttrInfo::is_common_info_size_method(callee_name_)) {
      return parent_->get_bs_field_value()  + ".size()";
    } else {
      return parent_->get_bs_field_value() + ".Get(" + env_ptr_->get_last_loop_var() + ")";
    }
  } else if (is_decl_ref_expr()) {
    std::string origin_expr_str = tool::fix_ad_enum(stmt_to_string(origin_expr_));
    if (is_item_type_enum()) {
      return std::string("bs::ItemType::") + get_ad_enum_name();
    } else if (is_ad_enum()) {
      if (env_ptr_->find(origin_expr_str) != nullptr) {
        return origin_expr_str;
      } else {
        return std::string("::bs::") + origin_expr_str;
      }
    } else {
      return origin_expr_str;
    }
  } else {
    return stmt_to_string(expr_);
  }
}

std::string ExprInfo::get_bs_field_value_normal() const {
  std::string bs_enum_str = get_bs_enum_str();
  std::ostringstream oss;

  // adlog.item_size() 特殊处理
  if (bs_enum_str == "adlog_item_size") {
    return "bslog.item_size()";
  }

  if ((callee_name_ == "size" || callee_name_ == "length") &&
      is_from_repeated_common_info()) {
    return parent_->get_bs_field_value_normal() + "." + callee_name_ + "()";
  }

  bool is_expr_from_list = is_from_list();
  bool is_expr_from_map = is_from_map();

  // common_info 逻辑不一样，从类型看都包含 repeated，但是其类型是从调用函数来区分的。
  // xxx_value 表示单值，xxx_list_value 表示 list, map_xxx_yyy_value 表示 map。
  if (is_from_repeated_common_info()) {
    if (callee_name_.find("list") != std::string::npos) {
      is_expr_from_list = true;
    } else if (callee_name_.find("map") != std::string::npos) {
      is_expr_from_map = true;
    }
  }

  const absl::optional<NewVarDef>& var_def = env_ptr_->find_new_def(bs_enum_str);
  if (!var_def) {
    LOG(INFO) << "cannot find var def in env, return empty str, expr: " << stmt_to_string(expr_)
              << ", bs_enum_str: " << bs_enum_str;
    return "";
  }

  LOG(INFO) << "expr: " << origin_expr_str()
            << ", is_from_repeated_common_info: " << is_from_repeated_common_info()
            << ", is_loop_var_ref: " << is_loop_var_ref();
  if (is_from_repeated_common_info()) {
    // common info 的下标目前写死为 idx
    // loop var
    if (is_loop_var_ref()) {
      if (const auto& loop_info = env_ptr_->get_loop_info()) {
        if (loop_info->loop_stage() == LoopStage::BODY) {
          if (!loop_info->is_for_stmt()) {
            return origin_expr_str();
          }
        }
      }
    }

    // 引用的变量
    if (is_decl_ref_expr() && is_str_type()) {
      return origin_expr_str();
    }

    if (is_common_info_method()) {
      if (callee_name_ == "first") {
        oss << var_def->name()
            << ".GetKey(idx)";
      } else if (callee_name_ == "second") {
        oss << var_def->name()
            << ".GetValue(idx)";
      } else if (is_common_info_list_method()) {
        if (contains_loop_var()) {
          if (const auto& loop_info = env_ptr_->get_loop_info()) {
            if (loop_info->is_for_stmt()) {
              oss << var_def->name() << ".Get(" << loop_info->loop_var() << ")";
            } else {
              oss << var_def->name() << ".Get(idx)";
            }
          } else {
            LOG(INFO) << "cannot get loop_info!";
          }
        } else if (call_expr_params_.size() > 0 && call_expr_params_[0] != nullptr) {
          // 参数来自模板参数或者属性
          oss << var_def->name()
              << ".Get(" << tool::trim_this(call_expr_params_[0]->origin_expr_str()) << ")";
        } else {
          oss << var_def->name();
        }
      } else {
        LOG(INFO) << "expr: " << origin_expr_str()
                  << ", name: " << var_def->name();
        oss << var_def->name();
      }
    } else if (is_common_info_size_method()) {
      oss << var_def->name() << ".size()";
    } else {
      if (callee_name_ == "first") {
        oss << var_def->name() << ".GetKey(idx)";
      } else if (callee_name_ == "second") {
        oss << var_def->name() << ".GetValue(idx)";
      } else {
        oss << var_def->name();
      }
    }
  } else if (is_expr_from_list) {
    if (contains_loop_var()) {
      oss << var_def->name() << ".Get(" << env_ptr_->get_last_loop_var() << ")";
    } else if (absl::optional<std::string> int_param = find_int_param()) {
      oss << var_def->name() << ".Get(" << *int_param << ")";
    } else if (env_ptr_->is_in_loop_body()) {
      // 不是很准确，还需要更准确。
      oss << var_def->name() << ".Get(idx)";
    } else {
      LOG(INFO) << "cannot find int_param, return var name";
      oss << var_def->name();
    }
  } else if (is_expr_from_map && !is_from_action_detail_map() && is_basic()) {
    if (const auto& loop_info = env_ptr_->cur_loop_info()) {
      if (callee_name_ == "first") {
        if (loop_info->is_for_stmt()) {
          oss << var_def->name() << ".GetKey(" << env_ptr_->get_last_loop_var() << ")";
        } else {
          oss << var_def->name() << ".GetKey(idx)";
        }
      } else if (callee_name_ == "second") {
        if (loop_info->is_for_stmt()) {
          oss << var_def->name() << ".GetValue(" << env_ptr_->get_last_loop_var() << ")";
        } else {
          oss << var_def->name() << ".GetValue(idx)";
        }
      } else {
        LOG(INFO) << "unsupported map expr: " << stmt_to_string(expr_)
                  << ", must be xxx.first or xxx.second";
      }
    } else {
      LOG(INFO) << "cannot find cur loop_info, expr: " << to_string();
    }
  } else {
    // 为了防止直接替换每行代码太长, 每个 scalar 都提前定义一个变量, 变量名取 bs_enum_str 结尾的部分,
    // 将定义保存在 env 中。
    oss << var_def->name();
  }

  LOG(INFO) << "expr: " << origin_expr_str()
            << ", str: " << oss.str();

  return oss.str();
}

std::string ExprInfo::get_bs_field_value_loop_var_size() const {
  if (!is_loop_var_size_method()) {
    return "";
  }

  if (parent_ == nullptr) {
    return "";
  }

  std::string bs_enum_str = parent_->get_bs_enum_str();

  const absl::optional<NewVarDef> &var_def = env_ptr_->find_new_def(bs_enum_str);
  if (!var_def) {
    LOG(INFO) << "cannot find var def in env, return empty str, expr: "
              << stmt_to_string(expr_) << ", bs_enum_str: " << bs_enum_str;
    return "";
  }

  if (const auto& loop_info = env_ptr_->get_loop_info()) {
    std::ostringstream oss;
    oss << var_def->name() << ".Get(";
    if (loop_info->is_for_stmt()) {
      oss << loop_info->loop_var();
    } else {
      oss << "idx";
    }
    oss << ").size()";

    return oss.str();
  }

  return "";
}

std::string ExprInfo::get_bs_field_value_query_token() const {
  LOG(INFO) << "expr: " << origin_expr_str()
            << ", callee_name: " << callee_name_
            << ", is_cxx_member_call_expr: " << is_cxx_member_call_expr();
  if (parent_ == nullptr) {
    return origin_expr_str();
  }

  if (is_proto_map_string_float_ref()) {
    return origin_expr_str();
  }

  if (is_query_token_call()) {
    return "std::move(BSGetQueryToken(bs))";
  }

  if (is_photo_text_call()) {
    return "std::move(BSGetPhotoText(bs, pos))";
  }

  std::string parent_str = parent_->get_bs_field_value_query_token();
  LOG(INFO) << "expr: " << origin_expr_str()
            << ", parent: " << parent_str
            << ", callee_name: " << callee_name_;
  std::ostringstream oss;
  if (callee_name_ == "first") {
    // 特殊处理, for 循环一开始会添加变量 auto query_key = query_token.GetKey(idx);
    oss << "query_key";
  } else if (callee_name_ == "second") {
    oss << parent_str << ".GetValue(idx)";
  } else if (callee_name_ == "operator->") {
    oss << parent_str;
  } else if (callee_name_ == "size") {
    oss << parent_str << ".size()";
  } else if (callee_name_ == "c_str") {
    oss << parent_str << ".data()";
  } else if (callee_name_ == "empty") {
    oss << parent_str << ".is_empty()";
  } else if (callee_name_.size() > 0) {
    oss << parent_str << "." << callee_name_ << "()";
  } else {
    oss << parent_str;
  }

  return oss.str();
}

std::string ExprInfo::get_bs_field_value_reco_user_info() const {
  std::string text = origin_expr_str();
  static std::regex p("^(adlog|ad_log)");
  return std::regex_replace(text, p, "bslog");
}

std::string ExprInfo::get_bs_middle_node_leaf() const {
  std::string bs_enum_str = get_bs_enum_str();
  std::vector<std::string> arr = absl::StrSplit(bs_enum_str, "_");

  std::ostringstream oss;

  bool is_has = bs_enum_str.find("exists") != std::string::npos;
  if (is_has) {
    oss << "BSHas";
  } else {
    oss << "BSGet";
  }

  oss << get_middle_node_root_name();
  for (const std::string& s: arr) {
    if (starts_with(s, "Get") || s == "exists" || s.size() == 0) {
      continue;
    }

    oss << char(toupper(s[0])) << s.substr(1);
  }

  return oss.str();
}

std::string ExprInfo::get_bs_middle_node_leaf_trim_size() const {
  std::string leaf = get_bs_middle_node_leaf();
  if (ends_with(leaf, "Size")) {
    return leaf.substr(0, leaf.size() - 4);
  }

  return leaf;
}

std::string ExprInfo::get_middle_node_field() const {
  return get_adlog_field_str();
}

std::string ExprInfo::get_bs_field_value_middle_node() const {
  std::string bs_enum_str = get_bs_enum_str();

  if (const auto& loop_info = env_ptr_->get_loop_info()) {
    if (const auto& var = env_ptr_->find_new_def(bs_enum_str)) {
      if (var->name().size() > 0) {
        if (loop_info->is_for_stmt()) {
          return var->name() + ".Get(" + env_ptr_->get_last_loop_var() + ")";
        } else if (loop_info->is_middle_node_proto_list_loop()) {
          return var->name() + ".Get(idx)";
        } else {
          LOG(INFO) << "new var name is empty, bs_enum_str: " << bs_enum_str
                    << ", expr: " << to_string();
          return "";
        }
      } else {
        LOG(INFO) << "cannot find new def, bs_enum_str: " << bs_enum_str
                  << ", expr: " << to_string();
        return "";
      }
    }
  }

  if (is_string() && !is_from_list()) {
    if (const auto& var = env_ptr_->find_new_def(bs_enum_str)) {
      if (var->name().size() > 0) {
        return var->name();
      } else {
        LOG(INFO) << "var->name() is empty, bs_enum_str: " << bs_enum_str
                  << ", expr: " << origin_expr_str();
      }
    } else {
      LOG(INFO) << "cannot find middle node str def in env, bs_enum_str: " << bs_enum_str
                << ", expr: " << origin_expr_str();
    }

    return "";
  }

  return get_bs_middle_node_leaf() + std::string("(bs, pos)");
}

std::string ExprInfo::get_bs_field_value_action_detail_leaf(const std::string& param) const {
  if (!is_action_detail_leaf()) {
    return "";
  }

  std::string bs_enum_str = get_bs_enum_str();
  std::ostringstream oss;

  const absl::optional<NewVarDef> &var_def = env_ptr_->find_new_def(bs_enum_str);
  if (!var_def) {
    LOG(INFO) << "cannot find var def in env, return empty str, expr: "
              << stmt_to_string(expr_) << ", bs_enum_str: " << bs_enum_str;
    return "";
  }

  oss << var_def->name() << ".Get(" << param << ")";

  return oss.str();
}

bool ExprInfo::is_action_detail_map_size_method() const {
  return callee_name_ == "explore_long_term_ad_action_size";
}

std::string ExprInfo::get_bs_field_value_general_proto_list_size_method() const {
  std::string bs_enum_str = get_bs_enum_str();
  if (ends_with(bs_enum_str, "_size")) {
    std::string new_bs_enum_str =
      bs_enum_str.substr(0, bs_enum_str.size() - std::string("_size").size());
    if (const auto& var_def = env_ptr_->find_new_def(new_bs_enum_str)) {
      return var_def->name() + ".size()";
    }
  }

  return "";
}

std::string ExprInfo::get_bs_field_value_middle_node_leaf_list_size_method() const {
  return get_bs_field_value_general_proto_list_size_method();
}

clang::Expr* ExprInfo::find_action_detail_index_param() const {
  if (!is_action_detail_leaf()) {
    return nullptr;
  }

  if (parent_ != nullptr && parent_->call_expr_params_size() > 1) {
    return parent_->call_expr_param(1)->expr();
  }

  return nullptr;
}

// bs 单值定义
// 如: auto key_item_type = BSFieldEnum::adlog_item_type;
//     int64_t item_type = BSFieldHelper::GetSingular<int64_t>(*bs, key_item_type, pos);
std::string ExprInfo::get_bs_scalar_def() const {
  std::string bs_enum_str = get_bs_enum_str();
  std::string new_name = env_ptr_->find_valid_new_name(bs_enum_str);
  return get_bs_scalar_def_helper(false, new_name);
}

std::string ExprInfo::get_bs_scalar_def(const std::string& name) const {
  return get_bs_scalar_def_helper(false, name);
}

std::string ExprInfo::get_bs_scalar_exists_def() const {
  std::string bs_enum_str = get_bs_enum_str();
  std::string new_name = env_ptr_->find_valid_new_name(bs_enum_str);
  return get_bs_scalar_def_helper(true, new_name);
}

std::string ExprInfo::get_bs_scalar_exists_def(const std::string& name) const {
  return get_bs_scalar_def_helper(true, name);
}

std::string ExprInfo::get_bs_scalar_def_helper(bool is_exists_expr,
                                              const std::string& name) const {
  std::ostringstream oss;

  std::string bs_enum_str = get_bs_enum_str();
  const auto& qual_type = expr_->getType();
  std::string type_str = tool::get_builtin_type_str(qual_type);
  std::string new_name = name;
  std::string enum_new_name = std::string("enum_") + new_name;

  if (is_exists_expr) {
    type_str = "bool";
    new_name = tool::get_exists_name(new_name);
    enum_new_name = tool::get_exists_name(enum_new_name);
  }

  oss << "    auto " << enum_new_name << " = BSFieldEnum::" << bs_enum_str << ";\n    ";
  oss << type_str << " " << new_name << " = BSFieldHelper::";

  if (is_exists_expr) {
    oss << "HasSingular";
  } else {
    oss << "GetSingular";
  }

  oss << "<" << type_str;

  if (env_ptr_->is_combine_feature() && !tool::is_item_field(bs_enum_str)) {
    oss << ", true";
  }
  oss << ">"
      << "(*bs, " << enum_new_name;

  if (is_exists_expr ||!env_ptr_->is_user_feature()) {
    oss << ", pos";
  }
  oss << ")";

  return oss.str();
}

// bs list 定义。
// 如: BSReaptedField<int64_t> adlog_item_xxx(*bs, enum_str, pos);
std::string ExprInfo::get_bs_list_def() const {
  std::ostringstream oss;

  const auto& qual_type = expr_->getType();
  const std::string& feature_type = env_ptr_->feature_type();
  std::string bs_enum_str = get_bs_enum_str();;
  std::string new_name = env_ptr_->find_valid_new_name(bs_enum_str);
  std::string enum_new_name = std::string("enum_") + new_name;

  std::string type_str;
  if (is_repeated_proto_type()) {
    if (absl::optional<std::string> type_str_opt = tool::get_repeated_proto_inner_type(qual_type)) {
      type_str = *type_str_opt;
    } else {
      LOG(INFO) << "cannot find list inner type str, expr: " << origin_expr_str()
                << ", type_str: " << qual_type.getAsString();
    }
  } else {
    type_str = tool::get_builtin_type_str(qual_type);
  }

  if (type_str.size() > 0) {
    std::string tmpl_args = type_str;
    if (env_ptr_->is_combine_feature() && !tool::is_item_field(bs_enum_str)) {
      tmpl_args = type_str + ", true";
    }

    oss << "   auto " << enum_new_name << " = BSFieldEnum::" << bs_enum_str
        << ";\n";
    oss << "BSRepeatedField<" << tmpl_args << "> ";
    oss << new_name << "(*bs, " << enum_new_name;
    if (!env_ptr_->is_user_feature()) {
      oss << ", pos";
    }
    oss << ")";
  } else {
    LOG(INFO) << "cannot find list inner type str, expr: " << origin_expr_str()
              << ", type_str: " << qual_type.getAsString();
  }

  return oss.str();
}

std::pair<std::string, std::string> ExprInfo::get_map_kv_type() const {
  std::pair<std::string, std::string> res;

  if (clang::CXXMemberCallExpr* cxx_member_call_expr = dyn_cast<clang::CXXMemberCallExpr>(expr_)) {
    const clang::TemplateArgumentList* map_args =
      get_proto_map_args_list(cxx_member_call_expr->getType());
    if (map_args != nullptr && map_args->size() == 2) {
      res.first = map_args->get(0).getAsType().getAsString();
      res.second = map_args->get(1).getAsType().getAsString();
    }
  }

  return res;
}

// bs map 定义。
// 如: BSMapField<int64_t, int64_t> adlog_item_xxx(*bs, key_enum_str, value_enum_str, pos);
std::string ExprInfo::get_bs_map_def() const {
  std::ostringstream oss;

  // const auto& qual_type = expr_->getType();
  const std::string& feature_type = env_ptr_->feature_type();
  std::string bs_enum_str = get_bs_enum_str();
  std::string new_name = env_ptr_->find_valid_new_name(bs_enum_str);
  std::string enum_new_name_key = std::string("enum_") + new_name + "_key";
  std::string enum_new_name_value = std::string("enum_") + new_name + "_value";

  std::pair<std::string, std::string> kv_type = get_map_kv_type();
  bool is_not_item_field = !tool::is_item_field(bs_enum_str);

  oss << "    auto " << enum_new_name_key << " = BSFieldEnum::" << bs_enum_str << "_key;\n";
  oss << "    auto " << enum_new_name_value << " = BSFieldEnum::" << bs_enum_str << "_value;\n";
  oss << "BSMapField<" << kv_type.first
      << ", " << kv_type.second;
  if (env_ptr_->is_combine_feature() && is_not_item_field) {
    oss << ", true";
  }
  oss << "> ";
  oss << new_name << "(*bs, " << enum_new_name_key << ", " << enum_new_name_value;
  if (!env_ptr_->is_user_feature()) {
    oss << ", pos";
  }
  oss << ")";

  return oss.str();
}

bool ExprInfo::is_ignore_callee_name() const {
  return ignore_callee_.find(callee_name_) != ignore_callee_.end();
}

bool ExprInfo::is_keep_callee_name() const {
  auto it = ignore_callee_.find(callee_name_);
  if (it == ignore_callee_.end()) {
    return true;
  }

  return false;
}

std::string ExprInfo::get_bs_enum_str() const {
  // 忽略中间节点根节点
  if (is_middle_node_root()) {
    return "";
  }

  if (parent_ == nullptr) {
    std::string root_str = stmt_to_string(expr_);
    if (root_str == "ad_log") {
      return "adlog";
    } else {
      return root_str;
    }
  }

  // common_info 最后访问的节点，不处理，直接返回 common_info enum 对应的节点。
  if (tool::is_common_info_enum(parent_->expr()->getType())) {
    return parent_->get_bs_enum_str();
  }

  // middle node loop 遍历
  if ((callee_name_ == "begin" || callee_name_ == "end")) {
    if (is_from_middle_node() || is_from_implicit_loop_var()) {
      return parent_->get_bs_enum_str();
    }
  }

  // 固定写法
  // adlog.Get().time()
  if (callee_name_ == "Get" && parent_ != nullptr && parent_->is_adlog_root()) {
    return "adlog";
  }

  std::ostringstream oss;
  oss << parent_->get_bs_enum_str();
  if (cur_expr_str_.size() > 0) {
    if (callee_name_ == "item") {
      // 去掉 pos
      oss << "_" << "item";
    } else if (starts_with(callee_name_, "has_")) {
      // has_xxx
      if (is_proto2_has_method(callee_name_)) {
        oss << "_" << callee_name_;
      } else {
        oss << "_" << callee_name_.substr(4) << "_exists";
      }
    } else if (tool::is_common_info_enum(expr_->getType())) {
      // common_info
      oss << "_" << callee_name_;
      if (const auto& enum_value = env_ptr_->get_common_attr_int_value()) {
        oss << "_key_" << *enum_value;
      // } else if (const auto& enum_name = env_ptr_->get_common_attr_int_name()) {
      //   oss << "_key_" << *enum_name;
      }
    } else if (is_action_detail_find_expr()) {
      // 确定 action 后的其他 expr
      if (const auto& action_detail_info = env_ptr_->get_action_detail_info()) {
        if (absl::optional<int> action = get_action()) {
          LOG(INFO) << "find action value: " << *action;
          oss << "_" << "key_" << *action;
        } else {
          LOG(INFO) << "cannot find action, expr: " << origin_expr_str();
        }
      } else if (const auto& action_detail_fixed_info = env_ptr_->get_action_detail_fixed_info()) {
        oss << "_" << "key_" << action_detail_fixed_info->action();
      } else {
        LOG(INFO) << "cannot get action_detail info in env, expr: " << stmt_to_string(expr_);
      }
    } else if (is_keep_callee_name()) {
      oss << "_" << absl::StrJoin(cur_expr_str_, "_");
    }
  }

  return oss.str();
}

std::string ExprInfo::get_adlog_field_str() const {
  // 忽略中间节点根节点
  if (is_middle_node_root()) {
    return "";
  }

  if (parent_ == nullptr) {
    std::string root_str = stmt_to_string(expr_);
    if (root_str == "ad_log") {
      return "adlog";
    } else {
      return root_str;
    }
  }

  // common_info 最后访问的节点，不处理，直接返回 common_info enum 对应的节点。
  if (tool::is_common_info_enum(parent_->expr()->getType())) {
    return parent_->get_adlog_field_str();
  }

  // middle node loop 遍历
  if (callee_name_ == "begin" || callee_name_ == "end") {
    return parent_->get_adlog_field_str();
  }

  // 固定写法
  // adlog.Get().time()
  if (callee_name_ == "Get" && parent_ != nullptr && parent_->is_adlog_root()) {
    return "adlog";
  }

  std::ostringstream oss;
  oss << parent_->get_adlog_field_str();
  if (cur_expr_str_.size() > 0) {
    if (oss.str().size() > 0) {
      oss << ".";
    }

    if (callee_name_ == "item") {
      // 去掉 pos
      oss << "item";
    } else if (starts_with(callee_name_, "has_")) {
      // has_xxx
      if (is_proto2_has_method(callee_name_)) {
        oss << callee_name_;
      } else {
        oss << callee_name_.substr(4) << ".exists";
      }
    } else if (tool::is_common_info_enum(expr_->getType())) {
      // common_info
      oss << callee_name_;
      if (const auto& enum_value = env_ptr_->get_common_attr_int_value()) {
        oss << ".key:" << *enum_value;
      }
    } else if (is_repeated_common_info_size()) {
      oss << std::regex_replace(callee_name_, std::regex("_size$"), ".size");
    } else if (is_action_detail_find_expr()) {
      // 确定 action 后的其他 expr
      if (const auto& action_detail_info = env_ptr_->get_action_detail_info()) {
        if (absl::optional<int> action = get_action()) {
          oss << "key:" << *action;
        } else {
          LOG(INFO) << "cannot find action!";
        }
      } else if (const auto& action_detail_fixed_info = env_ptr_->get_action_detail_fixed_info()) {
        oss << "key:" << action_detail_fixed_info->action();
      }
    } else if (is_keep_callee_name()) {
      oss << absl::StrJoin(cur_expr_str_, ".");
    }
  }

  return oss.str();
}

std::string ExprInfo::origin_expr_str() const {
  if (origin_expr_ == nullptr) {
    return stmt_to_string(expr_);
  }

  return stmt_to_string(origin_expr_);
}

std::string ExprInfo::get_bs_exists_enum_str() const {
  std::string bs_enum_str = get_bs_enum_str();
  if (bs_enum_str.size() == 0) {
    LOG(INFO) << "cannot find bs_enum_str!";
    return "";
  }

  return bs_enum_str + std::string("_exists");
}

absl::optional<std::string> ExprInfo::get_common_info_prefix() const {
  if (!is_from_repeated_common_info()) {
    return absl::nullopt;
  }

  std::string s = get_bs_enum_str();
  if (s.find("_key_") != std::string::npos) {
    std::regex p("(.*)_key_(.*)");
    return absl::optional<std::string>(std::regex_replace(s, p, "$1"));
  } else {
    return absl::optional<std::string>(s);
  }
}

absl::optional<std::string> ExprInfo::get_common_info_prefix_adlog() const {
  if (is_from_repeated_common_info() || is_repeated_common_info_size()) {
    std::string s = get_adlog_field_str();

    if (is_repeated_common_info_size()) {
      static std::regex p_size("\\.size$");
      return absl::optional<std::string>(std::regex_replace(s, p_size, ""));
    } else {
      if (s.find(".key:") != std::string::npos) {
        std::regex p("(.*)\\.key:(.*)");
        return absl::optional<std::string>(std::regex_replace(s, p, "$1"));
      } else {
        return absl::optional<std::string>(s);
      }
    }
  }

  return absl::nullopt;
}

std::string ExprInfo::get_adlog_expr() const {
  return "";
}

// 普通 adlog 字段，递归后以 adlog 开头，如 adlog.item(pos).id()
bool ExprInfo::is_from_normal_adlog() const {
  if (parent_ == nullptr) {
    std::string expr_str = stmt_to_string(expr_);
    return expr_str == "adlog" || expr_str == "ad_log";
  }
  return parent_->is_from_normal_adlog();
}

bool ExprInfo::is_from_adlog_item() const {
  if (parent_ == nullptr) {
    std::string expr_str = stmt_to_string(expr_);
    return expr_str == "item";
  }

  return parent_->is_from_adlog_item();
}

// 中间节点，如 PhotoInfo, LiveInfo, 也是来自 adlog
bool ExprInfo::is_from_middle_node() const {
  if (parent_ == nullptr || is_parent_this()) {
    return tool::is_middle_node_root(get_first_caller_name());
  }

  return parent_->is_from_middle_node();
}

bool ExprInfo::is_decl_ref_expr() const {
  return origin_expr_ != nullptr;
}

std::string ExprInfo::get_first_decl_ref() const {
  if (is_decl_ref_expr()) {
    return stmt_to_string(origin_expr_);
  }

  if (parent_ != nullptr) {
    return parent_->get_first_decl_ref();
  }

  return "";
}

bool ExprInfo::is_iter_second() const {
  std::string expr_str = stmt_to_string(expr_);
  if (is_decl_ref_expr()) {
    expr_str = stmt_to_string(origin_expr_);
  }

  if (expr_str.find("->second") != std::string::npos) {
    return true;
  }

  return false;
}

// 中间节点跟节点
bool ExprInfo::is_middle_node_root() const {
  return tool::is_middle_node_root(stmt_to_string(expr_));
}

std::string ExprInfo::get_middle_node_root_name() const {
  std::string first_caller_name = get_first_caller_name();
  if (starts_with(first_caller_name, "Get") && ends_with(first_caller_name, "(adlog.item(pos))")) {
    static std::string other_str = "Get(adlog.item(pos))";
    return first_caller_name.substr(3, first_caller_name.size() - other_str.size());
  }

  if (starts_with(first_caller_name, "Get") && ends_with(first_caller_name, "(item)")) {
    static std::string other_str = "Get(item)";
    return first_caller_name.substr(3, first_caller_name.size() - other_str.size());
  }

  return "";
}

// 中间节点有任一节点来自 repeated common_info, 则认为是来自 common_info 节点, 如
// 1. attr.first
// 2. attr.second
// 3. attr.int_value()
// 4. adlog.item(pos).common_info_attr(i)
bool ExprInfo::is_from_repeated_common_info() const {
  if (tool::is_repeated_common_info(expr_->getType())) {
    return true;
  }

  if (tool::is_common_info_enum(expr_->getType()) && contains_loop_var()) {
    return true;
  }

  if (parent_ != nullptr) {
    return parent_->is_from_repeated_common_info();
  }

  return false;
}

// 中间节点有任一节点来自 action_detail, 则认为来自 action_detail, 如
// const auto& ad_dsp_action_detail = adlog.user_info().user_real_time_action().real_time_dsp_action_detail();
// 1. ad_dsp_action_detail.find(no)
// 2. auto it = ad_dsp_action_detail.find(no);
//    auto list = it->second.list;
// 3. list.size();
bool ExprInfo::is_from_action_detail_map() const {
  if (is_action_detail_map()) {
    return true;
  }

  if (parent_ != nullptr) {
    return parent_->is_from_action_detail_map();
  }

  return false;
}

bool ExprInfo::is_action_detail_map() const {
  if (clang::CXXMemberCallExpr* cxx_member_call_expr = dyn_cast<clang::CXXMemberCallExpr>(expr_)) {
    return tool::is_action_detail_map(expr_->getType());
  }

  return false;
}

// const auto& ad_dsp_action_detail = adlog.user_info().user_real_time_action().real_time_dsp_action_detail();
// auto iter = ad_dsp_action_detail.find(no);
bool ExprInfo::is_action_detail_find_expr() const {
  if (clang::CXXMemberCallExpr* cxx_member_call_expr = dyn_cast<clang::CXXMemberCallExpr>(expr_)) {
    if (clang::MemberExpr* callee = dyn_cast<clang::MemberExpr>(cxx_member_call_expr->getCallee())) {
      std::string callee_name = callee->getMemberDecl()->getNameAsString();
      if (parent_ != nullptr &&
          (callee_name == "find" || callee_name == "at") &&
          parent_->is_action_detail_map()) {
        return true;
      }
    }
  }

  return false;
}

bool ExprInfo::is_action_detail_list_expr() const {
  return false;
}

bool ExprInfo::is_action_detail_map_end() const {
  if (ends_with(stmt_to_string(expr_), ".end()")) {
    if (parent_ != nullptr && parent_->is_action_detail_map()) {
      return true;
    }
  }

  return false;
}

bool ExprInfo::is_action_info_type() const {
  return tool::is_action_info(expr_->getType());
}

bool ExprInfo::is_action_detail_leaf() const {
  if (is_cxx_member_call_expr()) {
    if (parent_->is_action_info_type()) {
      if (parent_->is_cxx_operator_call_expr()) {
        return true;
      }
    }
  }

  return false;
}

// 单个 action detail 或者多个 action_detail, 如果是多个 action_detail, 取 actioin_vec_ 的第一个。
absl::optional<int> ExprInfo::get_action() const {
  if (env_ptr_ == nullptr) {
    LOG(INFO) << "env_ptr is nullptr!";
    return absl::nullopt;
  }

  if (!is_action_detail_find_expr()) {
    LOG(INFO) << "is not action_detail find expr: " << raw_expr_str();
    return absl::nullopt;
  }

  LOG(INFO) << "expr: " << origin_expr_str()
            << ", get action";

  if (params_.size() == 1) {
    std::string arg = stmt_to_string(params_[0]);
    absl::optional<int> int_value = find_int_ref_value(arg);
    if (int_value) {
      return int_value;
    } else {
      return find_first_int_in_loop_expr(arg);
    }
  } else {
    LOG(INFO) << "expr has no param, cannot find action: " << stmt_to_string(expr_);
  }

  return absl::nullopt;
}

absl::optional<int> ExprInfo::find_int_ref_value(const std::string& name) const {
  clang::Expr* value_expr = env_ptr_->find(name);
  if (value_expr == nullptr) {
    return absl::nullopt;
  }

  std::string value_expr_str = stmt_to_string(value_expr);
  if (is_integer(value_expr_str)) {
    return absl::optional<int>(std::stoi(value_expr_str));
  }

  return find_int_ref_value(value_expr_str);
}

absl::optional<int> ExprInfo::find_first_int_in_loop_expr(const std::string& arg) const {
  if (env_ptr_->is_loop_var(arg)) {
    if (const auto& loop_info = env_ptr_->get_loop_info()) {
      if (!loop_info->is_for_stmt() && loop_info->is_int_list_member_loop()) {
        const std::vector<int> &int_list_member_values = loop_info->int_list_member_values();
        if (const absl::optional<int> &int_list_index = loop_info->int_list_index()) {
          if (int_list_member_values.size() > 0 &&
              *int_list_index < int_list_member_values.size()) {
            return absl::optional<int>(int_list_member_values[*int_list_index]);
          }
        }
      }

      if (loop_info->parent_env_ptr() != nullptr)  {
        if (const auto& parent_loop_info = loop_info->parent_env_ptr()->get_loop_info()) {
          if (!parent_loop_info->is_for_stmt() && parent_loop_info->is_int_list_member_loop()) {
            const std::vector<int> &int_list_member_values = parent_loop_info->int_list_member_values();
            if (const absl::optional<int> &int_list_index = parent_loop_info->int_list_index()) {
              if (int_list_member_values.size() > 0 && *int_list_index < int_list_member_values.size()) {
                return absl::optional<int>(int_list_member_values[*int_list_index]);
              } else {
                LOG(INFO) << "cannot find int_list_index, arg: " << arg;
              }
            }
          }
        }
      }
    }
  }

  // for 循环遍历 int list var
  // for (int i = 0; i < urb_type_array.size(); i++) {
  //   int urb_type = urb_type_array[i];
  // }
  if (clang::Expr* init_expr = env_ptr_->find(arg)) {
    std::string s = stmt_to_string(init_expr);
    std::regex p("([\\w_\\d]+)\\[([\\d\\w_]+)\\]");
    std::smatch m;
    if (std::regex_search(s, m, p)) {
      if (m.size() > 2) {
        std::string var_name = m[1];
        std::string index_name = m[2];
        if (env_ptr_->is_loop_var(index_name)) {
          if (clang::Expr* loop_var_init_expr = env_ptr_->find(var_name)) {
            std::string expr_str = stmt_to_string(loop_var_init_expr);
            std::vector<int> values = tool::get_int_list_values_from_init_str(expr_str);
            if (values.size() > 0) {
              return absl::optional<int>(values[0]);
              LOG(INFO) << "find first action from int list var: " << values[0]
                        << ", loop_var_name: " << var_name
                        << ", values: " << absl::StrJoin(values, ",");
            }
          }
        }
      }
    }
  }

  return absl::nullopt;
}

absl::optional<std::string> ExprInfo::get_action_detail_prefix_adlog() const {
  if (is_action_detail_map()) {
    std::string s = get_adlog_field_str();
    if (ends_with(s, ".")) {
      return s.substr(0, s.size() - 1);
    } else if (s.find(".key:") != std::string::npos) {
      std::regex p("(.*)\\.key:(.*)");
      return absl::optional<std::string>(std::regex_replace(s, p, "$1"));
    } else {
      return absl::optional<std::string>(s);
    }
  }

  return absl::nullopt;
}

bool ExprInfo::is_find_name_value() const {
  if (callee_name_ == "find" && params_.size() == 1) {
    std::string param = stmt_to_string(params_[0]);
    if (ends_with(param, ".name_value()")) {
      return true;
    }
  }

  return false;
}

std::string ExprInfo::get_common_info_multi_map_name() const {
  if (callee_name_ == "find" && params_.size() == 1) {
    if (parent_ != nullptr) {
      const std::vector<std::string>& expr_str = parent_->cur_expr_str();
      if (expr_str.size() == 1) {
        return expr_str[0];
      }
    }
  }

  return "";
}

std::string ExprInfo::get_common_info_multi_attr_name() const {
  if (callee_name_ == "find" && params_.size() == 1) {
    std::string param = stmt_to_string(params_[0]);
    if (ends_with(param, ".name_value()")) {
      return param.substr(0, param.find("."));
    }
  }

  return "";
}

// user_attr_map_.end()
bool ExprInfo::is_common_info_multi_map_end() const {
  if (auto& common_info_multi_map = env_ptr_->mutable_common_info_multi_map()) {
    const std::string& map_name = common_info_multi_map->map_name();
    if (ends_with(stmt_to_string(expr_), map_name + ".end()")) {
      return true;
    }
  }

  return false;
}

bool ExprInfo::is_common_info_multi_map_attr() const {
  if (auto& common_info_multi_map = env_ptr_->mutable_common_info_multi_map()) {
    if (stmt_to_string(origin_expr_) == common_info_multi_map->attr_name()) {
      return true;
    }
  }

  return false;
}

bool ExprInfo::is_common_info_scalar_method() const {
  return CommonAttrInfo::is_common_info_scalar_method(callee_name_);
}

bool ExprInfo::is_common_info_list_method() const {
  return CommonAttrInfo::is_common_info_list_method(callee_name_);
}

bool ExprInfo::is_common_info_map_method() const {
  return CommonAttrInfo::is_common_info_map_method(callee_name_);
}

bool ExprInfo::is_common_info_map_find_expr() const {
  if (parent_ != nullptr) {
    if (parent_->is_common_info_map_method() && callee_name_ == "find") {
      return true;
    }
  }

  return false;
}

bool ExprInfo::is_common_info_method() const {
  return CommonAttrInfo::is_common_info_method(callee_name_);
}

bool ExprInfo::is_common_info_size_method() const {
  return CommonAttrInfo::is_common_info_size_method(callee_name_);
}

bool ExprInfo::is_common_info_empty_method() const {
  if (parent_ != nullptr && parent_->is_common_info_method()) {
    if (is_cxx_member_call_expr() && callee_name_ == "empty") {
      return true;
    }
  }

  return false;
}

bool ExprInfo::is_common_info_list_size_method() const {
  return CommonAttrInfo::is_common_info_list_size_method(callee_name_);
}

bool ExprInfo::is_common_info_list_size_method_divide_by_int() const {
  LOG(INFO) << "expr: " << origin_expr_str();
  if (is_binary_op_expr()) {
    LOG(INFO) << "callee_name_: " << callee_name_
              << ", param size: " << call_expr_params_.size();
    if (callee_name_ == "%" && call_expr_params_.size() == 2) {
      auto param0 = call_expr_params_[0];
      auto param1 = call_expr_params_[1];
      if (param0 != nullptr && param1 != nullptr) {
        LOG(INFO) << "param1 is int: " << param1->is_integral()
                  << ", para1: " << param1->origin_expr_str();
        if (param0->is_common_info_list_size_method() && param1->is_integral()) {
          return true;
        }
      }
    }
  }

  return false;
}

bool ExprInfo::is_common_info_leaf_method() const {
  return CommonAttrInfo::is_common_info_leaf_method(callee_name_);
}

bool ExprInfo::is_common_info_name_value() const {
  return is_from_repeated_common_info() && callee_name_ == "name_value";
}

bool ExprInfo::is_name_value_method() const {
  return callee_name_ == "name_value";
}

bool ExprInfo::is_common_info_compare_int_value(Env* env_ptr) const {
  if (env_ptr == nullptr) {
    return false;
  }

  if (is_integral() ||
      (is_decl_ref_expr() &&
       (is_int_ref() || is_template_int_ref() || is_common_attr_info_enum()))) {
    if (const auto& if_info = env_ptr->cur_if_info()) {
      if (if_info->if_stage() == IfStage::COND) {
        if (const auto& binary_op_info = env_ptr->cur_binary_op_info()) {
          if (if_info->has_cond_var_type(ExprType::ADLOG_COMMON_INFO_NAME_VALUE)) {
            return true;
          }
        }
      }
    }
  }

  return false;
}

bool ExprInfo::is_repeated_common_info() const {
  return tool::is_repeated_common_info(expr_->getType());
}

bool ExprInfo::is_repeated_common_info_size() const {
  return tool::is_repeated_common_info_size(callee_name_);
}

bool ExprInfo::is_common_info_struct_type() const {
  return tool::is_common_info_struct(expr_->getType());
}

bool ExprInfo::is_common_info_map_end() const {
  if (is_cxx_member_call_expr()) {
    if (callee_name_ == "end") {
      if (parent_ != nullptr && parent_->is_common_info_map_method()) {
        return true;
      }
    }
  }

  return false;
}

bool ExprInfo::is_common_info_map_iter() const {
  LOG(INFO) << "expr: " << origin_expr_str()
            << ", is_map_proto_iterator: " << tool::is_map_proto_iterator(expr_->getType());
  if (tool::is_map_proto_iterator(expr_->getType())) {
    if (parent_ != nullptr) {
      // 必须是非 for 循环遍历的情况。
      if (parent_->parent() != nullptr) {
        LOG(INFO) << "parent is_common_info_map_method: " << parent_->is_common_info_map_method()
                  << ", parent: " << parent_->origin_expr_str()
                  << ", callee_name: " << parent_->callee_name();
        if (parent_->parent()->is_common_info_map_method()) {
          return true;
        }
      }
    }
  }

  return false;
}

bool ExprInfo::is_common_info_map_iter_second() const {
  if (is_member_expr()) {
    LOG(INFO) << "member expr: " << origin_expr_str()
              << ", callee_name: " << callee_name_;
    if (parent_ != nullptr) {
      LOG(INFO) << "parent is_common_info_map_iter: " << parent_->is_common_info_map_iter();
    }
    if (callee_name_ == "second") {
      if (parent_ != nullptr && parent_->is_common_info_map_iter()) {
        return true;
      }
    }
  }

  return false;
}

absl::optional<CommonInfoValueType> ExprInfo::get_common_info_value_type() const {
  if (!is_common_info_method()) {
    return absl::nullopt;
  }

  return CommonAttrInfo::find_value_type(callee_name_);
}

bool ExprInfo::is_adlog_root() const {
  std::string expr_str = stmt_to_string(origin_expr_);
  return is_decl_ref_expr() && (expr_str == "adlog"  || expr_str == "ad_log");
}

bool ExprInfo::is_item_type_enum() const {
  if (is_decl_ref_expr()) {
    return tool::is_item_type_enum(origin_expr_->getType());
  }

  return false;
}

bool ExprInfo::is_ad_callback_log_enum() const {
  if (is_decl_ref_expr()) {
    return tool::is_ad_callback_log_enum(origin_expr_->getType());
  }

  return false;
}

bool ExprInfo::is_ad_action_type_enum() const {
  if (is_decl_ref_expr()) {
    return tool::is_ad_action_type_enum(origin_expr_->getType());
  }

  return false;
}

bool ExprInfo::is_ad_enum() const {
  if (is_decl_ref_expr()) {
    return tool::is_ad_enum(origin_expr_->getType());
  }

  return false;
}

bool ExprInfo::is_item_field() const {
  std::string bs_enum_str = get_bs_enum_str();
  return tool::is_item_field(bs_enum_str);
}

bool ExprInfo::is_adlog_user_field() const {
  std::string bs_enum_str = get_bs_enum_str();
  return tool::is_adlog_user_field(bs_enum_str);
}

std::string ExprInfo::get_ad_enum_name() const {
  if (!is_ad_enum()) {
    return "";
  }

  std::vector<std::string> arr = absl::StrSplit(stmt_to_string(origin_expr_), "::");
  if (arr.size() == 0) {
    return "";
  } else {
    return arr.back();
  }
}

bool ExprInfo::is_first_param_adlog_item() const {
  if (call_expr_params_.size() > 0) {
    std::string s = call_expr_params_[0]->to_string();
    if (s == "adlog.item(pos)" || s == "item") {
      return true;
    }
  }

  return false;
}

bool ExprInfo::is_unary_expr() const {
  if (clang::UnaryOperator* unary_operator = dyn_cast<clang::UnaryOperator>(expr_)) {
    return true;
  }

  return false;
}

std::string ExprInfo::get_first_caller_name() const {
  if (is_first_param_adlog_item()) {
   return to_string();
  }

  if (parent_ == nullptr || is_parent_this()) {
    if (callee_name_.size() > 0) {
      return tool::trim_this(callee_name_);
    } else {
      return tool::trim_this(origin_expr_str());
    }
  } else {
    return parent_->get_first_caller_name();
  }
}

bool ExprInfo::is_from_adlog() const {
  return is_from_normal_adlog() ||
    is_from_adlog_item() ||
    is_from_middle_node() ||
    is_from_repeated_common_info() ||
    is_from_action_detail_map() ||
    is_from_seq_list() ||
    is_from_reco_user_info() ||
    is_seq_list_reco_proto_type() ||
    is_from_query_token() ||
    is_from_photo_text();
}

bool ExprInfo::is_from_list() const {
  LOG(INFO) << "expr: " << to_string()
            << ", tool::is_var_proto_list(expr_->getType()): " << tool::is_var_proto_list(expr_->getType())
            << ", is_item_ref: " << is_item_ref();

  if (tool::is_var_proto_list(expr_->getType())) {
    return true;
  }

  if (is_item_ref()) {
    // item 不认为是 list。
    return false;
  }

  if (contains_loop_var()) {
    return true;
  }

  if (is_var_proto_message_type() || is_basic()) {
    if ((is_cxx_member_call_expr() || is_member_expr()) && call_expr_params_.size() > 0) {
      if (call_expr_params_[0]->is_integral()) {
        return true;
      }
    }
  }

  if (parent_ != nullptr) {
    return parent_->is_from_list();
  }

  return false;
}

bool ExprInfo::is_from_map() const {
  if (tool::is_var_proto_map(expr_->getType())) {
    return true;
  }

  if (parent_ != nullptr) {
    return parent_->is_from_map();
  }

  return false;
}

bool ExprInfo::is_basic() const {
  return is_basic_type(expr_->getType());
}

bool ExprInfo::is_nullptr() const {
  std::string expr_str = stmt_to_string(expr_);
  return expr_str == "nullptr" || expr_str == "NULL" || expr_str == "__null";
}

bool ExprInfo::is_cxx_member_call_expr() const {
  if (clang::CXXMemberCallExpr* cxx_member_call_expr = dyn_cast<clang::CXXMemberCallExpr>(expr_)) {
    return true;
  }

  return false;
}

bool ExprInfo::is_call_expr() const {
  if (clang::CallExpr* call_expr = dyn_cast<clang::CallExpr>(expr_)) {
    return true;
  }

  return false;
}

// 需要被替换的单值类型。
// 中间节点不算。
// 来自 list 或者 map 的也不算。
bool ExprInfo::is_basic_scalar() const {
  if (is_decl_ref_expr() && tool::is_ad_enum(origin_expr_->getType())) {
    return false;
  }

  if (callee_name_ == "size") {
    return true;
  }

  // 有不同的情况, 需要区别对待
  // attr.name_value(): false
  if (is_cxx_member_call_expr() &&
      callee_name_ == "name_value" &&
      parent_ != nullptr &&
      parent_->is_from_repeated_common_info()) {
    return false;
  }

  if (is_from_list()) {
    return false;
  }

  // item.type(): true
  if (is_cxx_member_call_expr() && tool::is_ad_enum(expr_->getType())) {
    return true;
  }

  if (CommonAttrInfo::is_common_info_scalar_method(callee_name_)) {
    return true;
  }

  if (is_from_middle_node()) {
    return false;
  }

  if (is_from_action_detail_map()) {
    return false;
  }

  if (!is_basic()) {
    return false;
  }

  if (!is_from_adlog()) {
    return false;
  }

  if (callee_name_ == "item_size") {
    return false;
  }

  if (params_.size() > 0) {
    return false;
  }

  if (clang::MemberExpr* member_expr = dyn_cast<clang::MemberExpr>(expr_)) {
    return false;
  }

  return true;
}

absl::optional<std::string> ExprInfo::get_builtin_type_str() const {
  if (is_basic()) {
    return absl::optional<std::string>(tool::get_builtin_type_str(expr_->getType()));
  }

  return absl::nullopt;
}

// ::auto_cpp_rewriter::ContextInfoCommonAttr::MEDIUM_UID
bool ExprInfo::is_common_attr_info_enum() const {
  if (clang::DeclRefExpr* decl_ref_expr = dyn_cast<clang::DeclRefExpr>(expr_)) {
    return tool::is_common_info_enum(decl_ref_expr->getType());
  }

  return false;
}

absl::optional<int> ExprInfo::get_common_attr_int_value() const {
  if (clang::DeclRefExpr* decl_ref_expr = dyn_cast<clang::DeclRefExpr>(expr_)) {
    return find_common_attr_int_value(decl_ref_expr);
  }

  return absl::nullopt;
}

bool ExprInfo::contains_loop_var() const {
  for (size_t i = 0; i < params_.size(); i++) {
    if (env_ptr_->is_loop_var(stmt_to_string(params_[i]))) {
      return true;
    }
  }

  for (size_t i = 0; i < call_expr_params_.size(); i++) {
    if (call_expr_params_[i] != nullptr) {
      if (env_ptr_->is_loop_var(call_expr_params_[i]->origin_expr_str())) {
        return true;
      }
    }
  }

  if (parent_ != nullptr) {
    return parent_->contains_loop_var();
  }

  return false;
}

bool ExprInfo::has_decl_ref() const {
  if (is_decl_ref_expr()) {
    return true;
  }

  if (parent_ != nullptr) {
    return parent_->has_decl_ref();
  }

  return false;
}

bool ExprInfo::is_caller_custom_decl_ref() const {
  if (clang::CXXMemberCallExpr* cxx_member_call_expr = dyn_cast<clang::CXXMemberCallExpr>(expr_)) {
    clang::Expr* caller = cxx_member_call_expr->getImplicitObjectArgument();
    if (clang::DeclRefExpr* decl_ref_expr = dyn_cast<clang::DeclRefExpr>(caller)) {
      return stmt_to_string(decl_ref_expr) != "item";
    }
  }

  return false;
}

bool ExprInfo::is_caller_str_ref() const {
  if (clang::CXXMemberCallExpr* cxx_member_call_expr = dyn_cast<clang::CXXMemberCallExpr>(expr_)) {
    if (parent_ != nullptr && parent_->is_decl_ref_expr()) {
      return tool::is_string(parent_->origin_expr()->getType());
    }
  }

  return false;
}

bool ExprInfo::is_str_decl_ref() const {
    if (origin_expr_ != nullptr) {
      return tool::is_string(origin_expr_->getType());
  }

  return false;
}

bool ExprInfo::is_item_ref() const {
  return origin_expr_ != nullptr && stmt_to_string(origin_expr_) == "item";
}

bool ExprInfo::is_string() const {
  return tool::is_string(expr_->getType());
}

bool ExprInfo::is_integral() const {
  return expr_->getType().getTypePtr()->isIntegerType();
}

absl::optional<int> ExprInfo::get_int_value() const {
  if (expr_ == nullptr) {
    return absl::nullopt;
  }

  std::string s = stmt_to_string(expr_);
  if (is_integer(s)) {
    return absl::optional<int>(std::stoi(s));
  }

  return absl::nullopt;
}

absl::optional<int> ExprInfo::get_int_ref_value() const {
  if (origin_expr_ == nullptr) {
    return absl::nullopt;
  }

  std::string s = stmt_to_string(origin_expr_);
  clang::Expr* value_expr = env_ptr_->find(s);
  if (value_expr == nullptr) {
    return absl::nullopt;
  }

  return absl::nullopt;
}

bool ExprInfo::is_int_ref() const {
  absl::optional<int> int_ref_value = get_int_ref_value();
  return int_ref_value.has_value();
}

absl::optional<std::string> ExprInfo::get_ref_var_name() const {
  if (origin_expr_ == nullptr) {
    return absl::nullopt;
  }

  return absl::optional<std::string>(stmt_to_string(origin_expr_));
}

bool ExprInfo::is_template_int_ref() const {
  if (origin_expr_ == nullptr) {
    return false;
  }

  absl::optional<std::string> int_name = env_ptr_->get_template_int_name(origin_expr_);

  return int_name.has_value();
}

bool ExprInfo::is_common_info_enum_member_ref() const {
  clang::Expr* inner_expr = tool::get_inner_expr(expr_);
  if (inner_expr == nullptr) {
    return false;
  }

  if (clang::MemberExpr* member_expr = dyn_cast<clang::MemberExpr>(inner_expr)) {
    if (is_parent_this()) {
      if (const auto feature_info = env_ptr_->get_feature_info()) {
        std::string expr_str = tool::trim_this(stmt_to_string(inner_expr));
        if (feature_info->is_common_info_enum_member(expr_str, inner_expr->getType())) {
          return true;
        }
      }
    }
  }

  return false;
}

std::string ExprInfo::to_string() const {
  return stmt_to_string(expr_);
}

bool ExprInfo::is_get_norm_query() const {
  return starts_with(to_string(), "this->GetNormQuery");
}

bool ExprInfo::contains_template_parameter() const {
  for (size_t i = 0; i < params_.size(); i++) {
    if (absl::optional<std::string> int_name = env_ptr_->get_template_int_name(params_[i])) {
      LOG(INFO) << "i: " << i << ", int_name: " << *int_name << ", contains template";
      return true;
    }
  }

  if (parent_ != nullptr) {
    return parent_->contains_template_parameter();
  }

  return false;
}

absl::optional<std::string> ExprInfo::get_template_action() const {
  if (is_action_detail_find_expr()) {
    if (params_.size() > 0) {
      return env_ptr_->get_template_int_name(params_[0]);
    }
  }

  return absl::nullopt;
}

absl::optional<std::string> ExprInfo::get_action_detail_field_name() const {
  if (is_action_detail_map()) {
    return absl::optional<std::string>("");
  }

  if (parent_ == nullptr) {
    return absl::nullopt;
  }

  if (is_ignore_callee_name()) {
    return parent_->get_action_detail_field_name();
  }

  absl::optional<std::string> parent_field = parent_->get_action_detail_field_name();
  if (!parent_field) {
    return absl::nullopt;
  }

  std::ostringstream oss;
  if (parent_field->size() > 0) {
    oss << *parent_field << "." << callee_name_;
  } else {
    oss << callee_name_;
  }

  return absl::optional<std::string>(oss.str());
}

bool ExprInfo::is_var_proto_list() const {
  return tool::is_var_proto_list(expr_->getType());
}

bool ExprInfo::is_var_proto_map() const {
  return tool::is_var_proto_map(expr_->getType());
}

bool ExprInfo::is_parent_str_type() const {
  return parent_ != nullptr && parent_->is_str_type();
}

bool ExprInfo::is_parent_str_ref() const {
  return parent_ != nullptr && parent_->is_str_decl_ref();
}

bool ExprInfo::is_caller_loop_var() const {
  if (is_cxx_member_call_expr()) {
    if (parent_ != nullptr) {
      std::string parent_caller = parent_->origin_expr_str();
      if (env_ptr_->get_last_loop_var() == parent_caller) {
        return true;
      }
    }
  }

  return false;
}

std::string ExprInfo::callee_with_params(const StrictRewriter& rewriter) const {
  if (callee_name_.size() > 0) {
    std::ostringstream oss;

    // string 的 c_str() 固定替换为 data()
    if (callee_name_ == "c_str") {
      oss << "data(";
    } else {
      oss << callee_name_ << "(";
    }

    std::vector<std::string> args;
    if (params_.size() > 0) {
      for (size_t i = 0; i < params_.size(); i++) {
        if (tool::is_cxx_default_arg_expr(params_[i])) {
          continue;
        }
        args.push_back(rewriter.getRewrittenText(params_[i]));
      }
    }

    oss << absl::StrJoin(args, ",") << ")";

    return oss.str();
  }

  return "";
}

void ExprInfo::add_call_expr_param(std::shared_ptr<ExprInfo> expr_info_ptr) {
  call_expr_params_.push_back(expr_info_ptr);
}

ExprInfo* ExprInfo::call_expr_param(size_t index) const {
  if (index >= call_expr_params_.size()) {
    return nullptr;
  }

  return call_expr_params_[index].get();
}

bool ExprInfo::is_cxx_operator_call_expr() const {
  if (expr_ != nullptr) {
    if (clang::CXXOperatorCallExpr* cxx_operator_call_expr = dyn_cast<clang::CXXOperatorCallExpr>(expr_)) {
      return true;
    }
  }

  return false;
}

bool ExprInfo::is_cxx_operator_call_expr_deref() const {
  if (expr_ != nullptr) {
    if (clang::CXXOperatorCallExpr *cxx_operator_call_expr = dyn_cast<clang::CXXOperatorCallExpr>(expr_)) {
      std::string op = stmt_to_string(cxx_operator_call_expr->getCallee());
      if (op == "operator*") {
        return true;
      }
    }
  }

  return false;
}

NewVarType ExprInfo::get_new_var_type() const {
  if (is_from_list()) {
    return NewVarType::LIST;
  } else if (is_from_map()) {
    return NewVarType::MAP;
  } else {
    return NewVarType::SCALAR;
  }
}

bool ExprInfo::has_common_attr_int_value_in_env() const {
  if (absl::optional<int> enum_value = env_ptr_->get_common_attr_int_value()) {
    return true;
  }

  return false;
}

bool ExprInfo::has_common_attr_int_value_in_expr() const {
  if (absl::optional<int> enum_value = get_common_attr_int_value_in_expr()) {
    return true;
  }

  return false;
}

// 枚举或者 int
absl::optional<int> ExprInfo::get_common_attr_int_value_in_expr() const {
  if (is_common_attr_info_enum()) {
    return get_common_attr_int_value();
  } else if (is_integral()) {
    if (auto int_value = get_int_value()) {
      return absl::optional<int>(*int_value);
    }
  }

  return absl::nullopt;
}

absl::optional<std::string> ExprInfo::get_common_info_enum_name_in_expr() const {
  if (clang::DeclRefExpr* decl_ref_expr = dyn_cast<clang::DeclRefExpr>(expr_)) {
    return find_common_attr_enum_name_from_expr(decl_ref_expr);
  }

  return absl::nullopt;
}

bool ExprInfo::is_int_list_member_ref() const {
  if (const auto& feature_info = env_ptr_->get_feature_info()) {
    if (feature_info->is_int_list_member(expr_)) {
      return true;
    }
  }

  return false;
}

bool ExprInfo::is_int_list_var_ref() const {
  std::string name = tool::trim_this(origin_expr_str());
  if (clang::Stmt* stmt = env_ptr_->get_decl_stmt(name)) {
    if (clang::DeclStmt* decl_stmt = dyn_cast<clang::DeclStmt>(stmt)) {
      if (clang::VarDecl* var_decl = dyn_cast<clang::VarDecl>(decl_stmt->getSingleDecl())) {
        if (tool::is_int_vector(var_decl->getType())) {
          return true;
        }
      }
    }
  } else {
    LOG(INFO) << "cannot find decl_stmt in env, name: " << name;
  }

  return false;
}

bool ExprInfo::is_action_detail_list() const {
  if (callee_name_ == "list" && is_from_action_detail_map()) {
    return true;
  }

  return false;
}

bool ExprInfo::is_action_detail_list_size_expr() const {
  if (callee_name_ == "size") {
    if (parent_ != nullptr && parent_->is_action_detail_list()) {
      return true;
    }
  }

  return false;
}

std::string ExprInfo::get_bs_field_value_action_detail_list_size() const {
  std::string bs_enum_str = get_bs_enum_str();
  const absl::optional<NewVarDef>& var_def = env_ptr_->find_new_def(bs_enum_str);
  if (!var_def) {
    LOG(INFO) << "cannot find var def in env, return empty str, expr: " << stmt_to_string(expr_)
              << ", bs_enum_str: " << bs_enum_str;
    return "";
  }

  return var_def->name();
}

bool ExprInfo::is_parent_this() const {
  if (parent_ != nullptr && parent_->origin_expr_str() == "this") {
    return true;
  }

  return false;
}

bool ExprInfo::is_pos_ref() const {
  if (origin_expr_str() == "pos") {
    return true;
  }

  return false;
}

bool ExprInfo::is_has_reco_user_info_method() const {
  if (GlobalConfig::Instance()->rewrite_reco_user_info) {
    return false;
  } else {
    std::string expr_str = to_string();
    return expr_str == "adlog.has_reco_user_info()" || expr_str == "ad_log.has_reco_user_info()";
  }
}

bool ExprInfo::is_reco_user_info_method() const {
  if (GlobalConfig::Instance()->rewrite_reco_user_info) {
    return false;
  } else {
    std::string expr_str = to_string();
    return expr_str == "adlog.reco_user_info()" ||
      expr_str == "ad_log.reco_user_info()" ||
      expr_str == "ad_user_history_photo_embedding()";
  }
}

bool ExprInfo::is_from_reco_user_info() const {
  if (GlobalConfig::Instance()->rewrite_reco_user_info) {
    return false;
  } else {
    if (callee_name_ == "reco_user_info" || callee_name_ == "has_reco_user_info" || callee_name_ ==
    "ad_user_history_photo_embedding") {
      if (parent_ != nullptr && parent_->is_adlog_root()) {
        return true;
      } else {
        return false;
      }
    }

    if (parent_ != nullptr) {
      return parent_->is_from_reco_user_info();
    }

    return false;
  }
}

bool ExprInfo::is_from_reco_user_info_real() const {
  if (callee_name_ == "reco_user_info" ||
      callee_name_ == "has_reco_user_info" ||
      callee_name_ == "ad_user_history_photo_embedding") {
    if (parent_ != nullptr && parent_->is_adlog_root()) {
      return true;
    } else {
      return false;
    }
  }

  if (parent_ != nullptr) {
    return parent_->is_from_reco_user_info_real();
  }

  return false;
}

bool ExprInfo::is_action_info() const {
  return tool::is_action_info(expr_->getType());
}

bool ExprInfo::is_parent_action_info() const {
  if (parent_ != nullptr) {
    return parent_->is_action_info();
  }

  return false;
}

bool ExprInfo::is_repeated_action_info() const {
  return tool::is_repeated_action_info(expr_->getType());
}

bool ExprInfo::is_parent_repeated_action_info() const {
  if (parent_ != nullptr) {
    return parent_->is_repeated_action_info();
  }

  return false;
}

ExprInfo* ExprInfo::caller_info() const {
  if (caller_info_ != nullptr) {
    return caller_info_.get();
  } else {
    return nullptr;
  }
}

void ExprInfo::set_caller_info(std::shared_ptr<ExprInfo> caller_info) {
  caller_info_ = caller_info;
}

bool ExprInfo::is_seq_list_root() const {
  if (is_cxx_member_call_expr() || is_call_expr()) {
    std::string caller_name = get_first_caller_name();
    if (caller_name == "GetSeqList") {
      return true;
    }
  }

  return false;
}

bool ExprInfo::is_seq_list_root_ref() const {
  if (is_decl_ref_expr()) {
    std::string caller_name = get_first_caller_name();
    if (caller_name == "GetSeqList") {
      return true;
    }
  }

  return false;
}

// const auto& seq_list = *seq_list_ptr;
bool ExprInfo::is_seq_list_root_deref() const {
  if (clang::UnaryOperator* unary_operator = dyn_cast<clang::UnaryOperator>(expr_)) {
    auto op_str = clang::UnaryOperator::getOpcodeStr(unary_operator->getOpcode());
    std::string op(op_str.data(), op_str.size());
    if (op == "*") {
      std::string sub_expr = stmt_to_string(unary_operator->getSubExpr());
      clang::Expr* value_expr = env_ptr_->find(sub_expr);
      if (value_expr != nullptr) {
        std::string value_expr_str = tool::trim_this(stmt_to_string(value_expr));
        if (starts_with(value_expr_str, "GetSeqList")) {
          return true;
        }
      }
    }
  }

  return false;
}

bool ExprInfo::is_seq_list_ptr() const {
  return is_seq_list_root_ref();
}

bool ExprInfo::is_seq_list() const {
  return false;
}

bool ExprInfo::is_seq_list_reco_proto_type() const {
  if (is_seq_list_root()) {
    std::string caller_name = get_first_caller_name();
    if (const auto& feature_info = env_ptr_->get_feature_info()) {
      if (auto method_info = feature_info->find_method_info(caller_name)) {
        if (tool::is_reco_proto(method_info->return_type())) {
          return true;
        }
      }
    }
  }

  return false;
}

bool ExprInfo::is_address_expr() const {
  if (clang::UnaryOperator* unary_operator = dyn_cast<clang::UnaryOperator>(expr_)) {
    std::string op = clang::UnaryOperator::getOpcodeStr(unary_operator->getOpcode()).str();
    if (op == "&") {
      return true;
    }
  }

  return false;
}

bool ExprInfo::is_ptr_deref_expr() const {
  if (clang::UnaryOperator *unary_operator = dyn_cast<clang::UnaryOperator>(expr_)) {
    std::string op = clang::UnaryOperator::getOpcodeStr(unary_operator->getOpcode()).str();
    if (op == "*") {
      return true;
    }
  }

  return false;
}

bool ExprInfo::is_from_seq_list() const {
  if (is_seq_list() || is_seq_list_root() || is_seq_list_root_ref() || is_seq_list_root_deref()) {
    return true;
  }

  if (parent_ != nullptr) {
    return parent_->is_from_seq_list();
  }

  if (call_expr_params_.size() > 0) {
    return call_expr_params_[0]->is_from_seq_list();
  }

  return false;
}

bool ExprInfo::is_from_seq_list_reco() const {
  if (is_seq_list_reco_proto_type()) {
    return true;
  }

  if (parent_ != nullptr) {
    return parent_->is_from_seq_list_reco();
  }

  if (call_expr_params_.size() > 0) {
    return call_expr_params_[0]->is_from_seq_list_reco();
  }

  return false;
}

bool ExprInfo::is_general_adlog_var() const {
  if (callee_name_ == "size") {
    if (parent_ != nullptr && parent_->is_common_info_list_method()) {
      return false;
    }
  }

  if (is_from_adlog() &&
      is_basic() &&
      !is_method_is_train() &&
      !is_from_implicit_loop_var() &&
      (!is_cxx_operator_call_expr() || is_cxx_operator_call_expr_deref()) &&
      !is_from_reco_user_info() &&
      !is_from_seq_list() &&
      !is_from_middle_node() &&
      !is_from_query_token() &&
      !is_from_photo_text() &&
      !contains_template_parameter()) {
    return true;
  }

  return false;
}

bool ExprInfo::is_reco_proto_type() const {
  return tool::is_reco_proto(expr_->getType()) || is_seq_list_reco_proto_type();
}

bool ExprInfo::is_repeated_proto_type() const {
  return tool::is_repeated_proto(expr_->getType());
}

bool ExprInfo::is_repeated_proto_iterator_type() const {
  return tool::is_repeated_proto_iterator(expr_->getType());
}

bool ExprInfo::is_repeated_proto_ptr() const {
  return tool::is_repeated_proto_ptr(expr_->getType());
}

bool ExprInfo::is_map_repeated_int_list_type() const {
  return tool::is_map_repeated_int_list_type(expr_->getType());
}

bool ExprInfo::is_from_int_list_member() const {
  if (clang::MemberExpr* member_expr = dyn_cast<clang::MemberExpr>(expr_)) {
    std::string member_str = member_expr->getMemberDecl()->getNameAsString();
    if (const auto feature_info = env_ptr_->get_feature_info()) {
      if (feature_info->is_int_list_member(member_str)) {
        return true;
      }
    }
  }

  if (parent_ != nullptr) {
    return parent_->is_from_int_list_member();
  }

  return false;
}

bool ExprInfo::is_map_int_int_type() const {
  return tool::is_map_int_int_type(expr_->getType());
}

bool ExprInfo::is_str_type() const {
  return tool::is_string(expr_->getType());
}

bool ExprInfo::is_var_proto_message_type() const {
  return tool::is_var_proto_message(expr_->getType());
}

bool ExprInfo::is_repeated_proto_message_type() const {
  return tool::is_repeated_proto_message(expr_->getType());
}

bool ExprInfo::is_member_expr() const {
  if (clang::MemberExpr* member_expr = dyn_cast<clang::MemberExpr>(expr_)) {
    return true;
  }

  return false;
}

absl::optional<std::string> ExprInfo::find_int_list_member_name() const {
  if (is_member_expr()) {
    if (const auto feature_info = env_ptr_->get_feature_info()) {
      if (feature_info->is_int_list_member(callee_name_)) {
        return absl::optional<std::string>(callee_name_);
      }
    }
  }

  if (parent_ != nullptr) {
    return parent_->find_int_list_member_name();
  }

  return absl::nullopt;
}

bool ExprInfo::is_loop_iter_end() const {
  std::string s = to_string();
  return starts_with(s, "__") && ends_with(s, ".end()");
}

absl::optional<std::string> ExprInfo::get_adlog_field_str_after_loop_var() const {
  if (is_cxx_member_call_expr()) {
    std::string adlog_str = get_adlog_field_str();
    if (const auto& loop_info = env_ptr_->get_loop_info()) {
      const std::string& prefix = loop_info->prefix_adlog();
      if (prefix.size() > 0 &&
          (adlog_str.size() > prefix.size() + 1) &&
          starts_with(adlog_str, prefix)) {
        return absl::make_optional(adlog_str.substr(prefix.size() + 1));
      } else {
        LOG(INFO) << "cannot find prefix adlog for loop_var, expr: " << origin_expr_str();
        return absl::nullopt;
      }
    }
  }

  LOG(INFO) << "not cxx_member_call_expr, expr: " << origin_expr_str();
  return absl::nullopt;
}

bool ExprInfo::is_implicit_loop_var() const {
  return tool::is_implicit_loop_var(origin_expr_str());
}

bool ExprInfo::is_from_implicit_loop_var() const {
  return tool::is_from_implicit_loop_var(origin_expr_str());
}

bool ExprInfo::is_deref_implicit_loop_begin_expr() const {
  return tool::is_deref_implicit_loop_begin(origin_expr_str());
}

bool ExprInfo::is_loop_var_ref() const {
  if (env_ptr_->is_in_loop()) {
    if (env_ptr_->is_loop_var(origin_expr_str())) {
      return true;
    }

    if (clang::Expr* init_expr = env_ptr_->find(origin_expr_str())) {
      if (env_ptr_->is_loop_var(stmt_to_string(init_expr))) {
        return true;
      }
    }
  }

  return false;
}

bool ExprInfo::is_loop_var_size_method() const {
  if (parent_ != nullptr) {
    if (parent_->is_loop_var_ref() && callee_name_ == "size") {
      return true;
    }
  }

  return false;
}

bool ExprInfo::is_parent_loop_var_ref() const {
  if (parent_ != nullptr && parent_->is_loop_var_ref()) {
    return true;
  }

  return false;
}

bool ExprInfo::is_binary_op_expr() const {
  if (expr_ != nullptr) {
    if (clang::BinaryOperator* binary_operator = dyn_cast<clang::BinaryOperator>(expr_)) {
      return true;
    }
  }

  return false;
}

absl::optional<std::string> ExprInfo::find_int_param() const {
  // adlog.item(pos) 中的 pos 不是循环变量
  if (callee_name_ == "item" && parent_ != nullptr && parent_->is_adlog_root()) {
    return absl::nullopt;
  }

  if (call_expr_params_.size() == 1) {
    auto param = call_expr_params_[0];
    if (param->is_integral()) {
      return absl::make_optional(param->origin_expr_str());
    } else if (absl::optional<int> int_value = get_int_ref_value()) {
      return absl::make_optional(std::to_string(*int_value));
    }
  }

  if (parent_ != nullptr) {
    return parent_->find_int_param();
  }

  return absl::nullopt;
}

bool ExprInfo::is_list_size_method() const {
  if (is_from_adlog() && absl::EndsWith(callee_name_, "_size")) {
    return true;
  }

  return false;
}

bool ExprInfo::is_general_proto_list_size_method() const {
  if (is_repeated_common_info_size()) {
    return false;
  }

  if (is_basic()) {
    std::string bs_enum_str = get_bs_enum_str_trim_size();
    if (starts_with(bs_enum_str, "adlog")) {
      if (bs_enum_str.size() > 0) {
        if (const auto feature_info = env_ptr_->get_feature_info()) {
          if (feature_info->is_in_bs_enum_var_type(bs_enum_str)) {
            return true;
          }
        }
      }
    }
  }

  return false;
}

bool ExprInfo::is_middle_node_leaf_list_size_method() const {
  if (is_repeated_common_info_size()) {
    return false;
  }

  if (is_from_middle_node()) {
    if (is_basic()) {
      std::string bs_enum_str = get_bs_enum_str_trim_size();
      if (!starts_with(bs_enum_str, "adlog")) {
        if (bs_enum_str.size() > 0) {
          if (const auto feature_info = env_ptr_->get_feature_info()) {
            if (feature_info->is_in_middle_node_bs_enum_var_type(bs_enum_str)) {
              return true;
            }
          }
        }
      }
    }
  }

  return false;
}

std::string ExprInfo::get_bs_enum_str_trim_size() const {
  std::string bs_enum_str = get_bs_enum_str();
  if (ends_with(bs_enum_str, "_size")) {
    return bs_enum_str.substr(0, bs_enum_str.size() - 5);
  }

  return "";
}

bool ExprInfo::is_str_concat() const {
  if (is_cxx_operator_call_expr()) {
    if (callee_name_ == "operator+") {
      if (call_expr_params_.size() == 2) {
        if (auto param0 = call_expr_params_[0]) {
          if (param0->is_string()) {
            return true;
          }
        }
      }
    }
  }

  return false;
}

bool ExprInfo::is_cxx_functional_cast_expr() const {
  if (clang::CXXFunctionalCastExpr* cxx_functional_cast_expr =
      dyn_cast<clang::CXXFunctionalCastExpr>(expr_)) {
    return true;
  }

  return false;
}

bool ExprInfo::is_enum_proto_call() const {
  static std::unordered_set<std::string> names = {
    "auto_cpp_rewriter::AdActionType_descriptor",
    "auto_cpp_rewriter::AdCallbackLog::EventType_descriptor",
    "auto_cpp_rewriter::class AdCallbackLog::EventType_descriptor",
    "auto_cpp_rewriter::AdActionType_Name",
    "auto_cpp_rewriter::AdActionType",
    "auto_cpp_rewriter::AdCallbackLog::EventType_Parse",
    "auto_cpp_rewriter::class AdCallbackLog::EventType_Parse"
  };

  if (is_call_expr() || is_cxx_functional_cast_expr()) {
    std::string s = origin_expr_str();
    for (const auto &name : names) {
      if (starts_with(s, name)) {
        return true;
      }
    }
  }

  return false;
}

bool ExprInfo::is_query_token_call() const {
  if (is_cxx_member_call_expr()) {
    std::string s = tool::trim_this(to_string());
    if (starts_with(s, "GetQueryToken")) {
      return true;
    }
  }

  return false;
}

bool ExprInfo::is_from_query_token() const {
  if (is_query_token_call()) {
    return true;
  }

  if (parent_ != nullptr) {
    return parent_->is_from_query_token();
  }

  return false;
}

bool ExprInfo::is_from_photo_text() const {
  if (is_photo_text_call()) {
    return true;
  }

  if (parent_ != nullptr) {
    return parent_->is_from_photo_text();
  }

  return false;
}

bool ExprInfo::is_parent_from_proto_map_string_float() const {
  if (is_cxx_member_call_expr() && parent_ != nullptr) {
    if (parent_->is_from_query_token() || parent_->is_from_photo_text()) {
      return true;
    }
  }

  return false;
}

bool ExprInfo::is_photo_text_call() const {
  if (is_cxx_member_call_expr()) {
    std::string s = tool::trim_this(to_string());
    if (starts_with(s, "GetPhotoText")) {
      return true;
    }
  }

  return false;
}

bool ExprInfo::is_proto_map_string_float_ptr_type() const {
  return tool::is_proto_map_string_float_ptr(expr_->getType());
}

bool ExprInfo::is_proto_map_string_float_ref() const {
  if (is_decl_ref_expr()) {
    if (is_query_token_call() || is_photo_text_call()) {
      return true;
    }
  }

  return false;
}

bool ExprInfo::is_proto_map_string_float_iter_type() const {
  return tool::is_proto_map_string_float_iter(expr_->getType());
}

bool ExprInfo::is_proto_map_string_float_iter_first() const {
  if (parent_ != nullptr && parent_->is_proto_map_string_float_iter_type()) {
    if (callee_name_ == "first") {
      return true;
    }
  }

  return false;
}

bool ExprInfo::is_proto_map_string_float_iter_second() const {
  if (parent_ != nullptr && parent_->is_proto_map_string_float_iter_type()) {
    if (callee_name_ == "second") {
      return true;
    }
  }

  return false;
}

bool ExprInfo::is_proto_map_string_float_size() const {
  if (parent_ != nullptr && parent_->is_proto_map_string_float_ptr_type()) {
    if (callee_name_ == "size") {
      return true;
    }
  }

  return false;
}

bool ExprInfo::is_proto_map_string_float_end() const {
  if (is_cxx_member_call_expr()) {
    if (callee_name_ == "end") {
      if (parent_ != nullptr && parent_->is_proto_map_string_float_ptr_type()) {
        return true;
      }
    }
  }

  return false;
}

bool ExprInfo::is_photo_text_find_expr() const {
  if (callee_name_ == "find") {
    if (is_from_photo_text()) {
      return true;
    }
  }

  return false;
}

clang::Expr *ExprInfo::get_proto_map_string_float_loop_var_expr() const {
  if (parent_ != nullptr && parent_->is_proto_map_string_float_ref()) {
    return parent_->origin_expr();
  }

  if (parent_ != nullptr) {
    return parent_->get_proto_map_string_float_loop_var_expr();
  }

  return nullptr;
}

clang::Expr* ExprInfo::get_loop_var_expr() const {
  if (tool::is_implicit_loop_var(origin_expr_str())) {
    return expr_;
  }

  if (parent_ != nullptr) {
    return parent_->get_loop_var_expr();
  }

  return nullptr;
}

bool ExprInfo::is_method_is_train() const {
  if (is_cxx_member_call_expr() && callee_name_ == "is_train") {
    return true;
  }

  return false;
}

bool ExprInfo::is_parent_common_info_map_method() const {
  if (parent_ != nullptr) {
    if (CommonAttrInfo::is_common_info_map_method(parent_->callee_name())) {
      return true;
    }
  }

  return false;
}

bool ExprInfo::is_proto2_has_method(const std::string& method_name) const {
  static std::unordered_set<std::string> names = {
    "has_car",
    "has_house"
  };

  return names.find(method_name) != names.end();
}

bool ExprInfo::is_char_arr_type() const {
  return tool::is_char_arr(expr_->getType());
}

bool ExprInfo::is_decl_init_expr() const {
  if (env_ptr_ != nullptr) {
    if (const auto& decl_info = env_ptr_->cur_decl_info()) {
      if (origin_expr_str() == stmt_to_string(decl_info->init_expr())) {
        return true;
      }
    }
  }

  return false;
}

bool ExprInfo::is_in_decl_stmt() const {
  if (const auto& decl_info = env_ptr_->cur_decl_info()) {
    return true;
  }

  return false;
}

bool ExprInfo::is_repeated_proto_list_leaf_type() const {
  return tool::is_repeated_proto_list_leaf_type(expr_->getType());
}

bool ExprInfo::is_bslog_field_enum_decl() const {
  if (env_ptr_ != nullptr) {
    if (is_decl_ref_expr()) {
      std::string var_name = origin_expr_str();
      if (clang::Stmt *decl_stmt = env_ptr_->get_decl_stmt(var_name)) {
        std::string decl_stmt_str = stmt_to_string(decl_stmt);

        if (decl_stmt_str.find("BSFieldEnum") != std::string::npos) {
          return true;
        }
      }
    }
  }

  return false;
}

bool ExprInfo::is_bslog_field_var_decl() const {
  if (env_ptr_ != nullptr) {
    if (is_decl_ref_expr()) {
      std::string var_name = origin_expr_str();
      return env_ptr_->is_bslog_field_var_decl(var_name);
    }
  }

  return false;
}

bool ExprInfo::is_bslog_field_enum_ref() const {
  std::string s = origin_expr_str();
  if (absl::StartsWith(s, "BSFieldEnum::")) {
    return true;
  }

  return is_bslog_field_enum_decl();
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
