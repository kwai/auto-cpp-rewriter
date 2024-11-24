#include <regex>
#include <sstream>
#include <string>

#include "absl/strings/str_split.h"
#include "../Tool.h"
#include "../Env.h"
#include "CommonInfoFixed.h"
#include "MiddleNodeInfo.h"
#include "../handler/StrictRewriter.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

std::string CommonInfoFixed::get_bs_wrap_text(Env* env_ptr) const {
  if (method_name_.size() == 0) {
    LOG(INFO) << "missing method_name for common info fixed, prefix: " << prefix_
              << ", int_name: " << int_name_;
    return "";
  }

  std::string pre_text = bs_pre_text_;

  std::string loop_text;
  if (bs_loop_text_) {
    loop_text = *bs_loop_text_;
  }

  bool is_list = CommonAttrInfo::is_common_info_list_method(method_name_);
  if (is_list && !is_for_stmt()) {
    if (list_loop_var_) {
      std::string var_name = get_bs_var_name(env_ptr);
      // std::regex p_list_loop_var(std::string("([^a-zA-Z0-9_])") +
      //                            *list_loop_var_ +
      //                            std::string("([^a-zA-Z0-9_])"));
      // loop_text = std::regex_replace(loop_text,
      //                                p_list_loop_var,
      //                                std::string("$1") + var_name + std::string("$2"));
      std::string loop_var_assign = std::string("auto ") +
                                    *list_loop_var_ +
                                    " = " + var_name;
      loop_text = loop_var_assign + ";\n" + loop_text;
    }
  }

  std::ostringstream oss;
  oss << "if (";

  oss << get_exists_expr(env_ptr) << ") {\n    ";

  if (is_scalar()) {
    oss << pre_text;
  } else if (is_for_stmt()) {
    oss << pre_text << "\n" << loop_text;
  } else if (has_list_method_address()) {
    oss << pre_text;
  } else {
    if (size_method_name() && !is_size_method_in_loop_init()) {
      oss << pre_text;
    } else {
      std::string bs_enum_str = get_bs_enum_str();
      if (const absl::optional<NewVarDef>& var = env_ptr->find_new_def(bs_enum_str)) {
        oss << pre_text
            << "for (size_t idx = 0; idx < " << var->name() << ".size(); idx++) {\n    "
            << loop_text
            << "\n}\n";
      } else {
        LOG(INFO) << "cannot find list or map var_name in env, bs_enum_str: " << bs_enum_str;
      }
    }
  }

  if (bs_post_text_) {
    oss << *bs_post_text_;
  }

  oss << "}\n\n";

  return oss.str();
}

std::string CommonInfoFixed::get_exists_expr(Env* env_ptr) const {
  if (method_name_.size() == 0) {
    LOG(INFO) << "cannot find common info method or value, prefix: " << prefix_;
    return "";
  }

  std::string inner_type_str = CommonAttrInfo::get_inner_type_str(value_type_);
  if (inner_type_str.size() == 0) {
    LOG(INFO) << "cannot get inner_type_str, prefix: " << prefix_;
  }

  std::ostringstream oss;
  std::string bs_enum_str = get_bs_enum_str();

  if (CommonAttrInfo::is_common_info_scalar_method(method_name_)) {
    oss << get_exists_functor_name() << "(bs, pos)";
  } else {
    if (const absl::optional<NewVarDef>& var = env_ptr->find_new_def(bs_enum_str)) {
      if (var->name().size() == 0) {
        LOG(INFO) << "cannot find common info var_name, prefix: " << prefix_;
        return "";
      }

      oss << "!" << var->name() << ".is_empty()";
    }
  }

  return oss.str();
}

std::string CommonInfoFixed::get_bs_enum_str() const {
  return prefix_ + "_" + tool::trim_tail_underscore(int_name_);
}

std::string CommonInfoFixed::get_bs_scalar_def(Env* env_ptr) const {
  if (method_name_.size() == 0) {
    LOG(INFO) << "common info fixed missing method_name!";
    return "";
  }

  if (env_ptr == nullptr) {
    LOG(INFO) << "env_ptr is nullptr!";
    return "";
  }

  std::string bs_enum_str = get_bs_enum_str();
  std::string new_name = env_ptr->find_valid_new_name(bs_enum_str);
  std::string type_str = CommonAttrInfo::get_inner_type_str(value_type_);

  std::ostringstream oss;
  oss << type_str << " " << new_name << " = " << get_functor_name() << "(bs, pos)";
  return oss.str();
}

std::string CommonInfoFixed::get_bs_list_def(Env* env_ptr) const {
  if (method_name_.size() == 0) {
    LOG(INFO) << "common info fixed missing method_name!";
    return "";
  }

  if (env_ptr == nullptr) {
    LOG(INFO) << "env_ptr is nullptr!";
    return "";
  }

  std::string bs_enum_str = get_bs_enum_str();
  std::string new_name = env_ptr->find_valid_new_name(bs_enum_str);
  std::string type_str = CommonAttrInfo::get_inner_type_str(value_type_);

  std::ostringstream oss;
  oss << "BSRepeatedField<" << type_str;
  if (!middle_node_info_) {
    if (env_ptr->is_combine_feature() && !tool::is_item_field(bs_enum_str)) {
      oss << ", true";
    }
  }
  oss << "> " << new_name << " = std::move(" << get_functor_name() << "(bs, pos))";

  return oss.str();
}

std::string CommonInfoFixed::get_bs_map_def(Env* env_ptr) const {
  if (method_name_.size() == 0) {
    LOG(INFO) << "common info fixed missing method_name!";
    return "";
  }

  if (env_ptr == nullptr) {
    LOG(INFO) << "env_ptr is nullptr!";
    return "";
  }

  std::string bs_enum_str = get_bs_enum_str();
  std::string new_name = env_ptr->find_valid_new_name(bs_enum_str);
  std::pair<std::string, std::string> map_type_str = CommonAttrInfo::get_map_inner_type_str(value_type_);

  std::ostringstream oss;
  oss << "BSMapField<" << map_type_str.first << ", " << map_type_str.second;
  if (!middle_node_info_) {
    if (env_ptr->is_combine_feature() && !tool::is_item_field(bs_enum_str)) {
      oss << ", true";
    }
  }
  oss << "> " << new_name << " = std::move(" << get_functor_name() << "(bs, pos))";

  return oss.str();
}

std::string CommonInfoFixed::get_bs_var_name(Env* env_ptr) const {
  std::string bs_enum_str = get_bs_enum_str();

  if (const absl::optional<NewVarDef>& var = env_ptr->find_new_def(bs_enum_str)) {
    if (is_scalar()) {
      return var->name();
    } else if (is_list()) {
      if (is_for_stmt()) {
        // 必定是遍历 list value
        const std::string &loop_var = env_ptr->get_last_loop_var();
        if (loop_var.size() > 0) {
          return var->name() + ".Get(" + loop_var + ")";
        } else {
          LOG(INFO) << "cannot find loop var for for_stmt!";
          return "";
        }
      } else {
        return var->name() + ".Get(idx)";
      }
    } else {
      // 应该用 get_map_bs_var_name
      return var->name() + ".GetKey(idx)";
    }
  } else {
    LOG(INFO) << "cannot find list or map var_name in env, bs_enum_str: " << bs_enum_str;
    return "";
  }
}

std::string CommonInfoFixed::get_bs_var_def_name(Env *env_ptr) const {
  std::string bs_enum_str = get_bs_enum_str();

  if (const absl::optional<NewVarDef> &var = env_ptr->find_new_def(bs_enum_str)) {
    return var->name();
  }

  LOG(INFO) << "cannot find list or map var_name in env, bs_enum_str: "
            << bs_enum_str;
  return "";
}

std::string CommonInfoFixed::get_map_bs_var_name(Env* env_ptr, const std::string& member) const {
  std::string bs_enum_str = get_bs_enum_str();

  if (const absl::optional<NewVarDef> &var = env_ptr->find_new_def(bs_enum_str)) {
    if (is_map()) {
      if (member == "first") {
        return var->name() + ".GetKey(idx)";
      } else {
        return var->name() + ".GetValue(idx)";
      }
    }
  } else {
    LOG(INFO) << "cannot find list or map var_name in env, bs_enum_str: " << bs_enum_str;
    return "";
  }

  return "";
}

std::string CommonInfoFixed::get_functor_name() const {
  std::ostringstream oss;

  std::string bs_enum_str = get_bs_enum_str();
  std::vector<std::string> arr = absl::StrSplit(bs_enum_str, "_");
  oss << "BSGet";

  if (middle_node_info_) {
    oss << middle_node_info_->name();
  }

  for (const std::string& s: arr) {
    if (starts_with(s, "adlog") || s == "exists" || s.size() == 0) {
      continue;
    }

    oss << char(toupper(s[0])) << s.substr(1);
  }

  return oss.str();
}

std::string CommonInfoFixed::get_exists_functor_name() const {
  std::string functor_name = get_functor_name();
  static std::regex p("BSGet");
  return std::regex_replace(functor_name, p, "BSHas");
}

// BSFixedCommonInfo<int64_t> BSGetItemAdDspInfoCommonInfoAttr(no);
std::string CommonInfoFixed::get_bs_scalar_field_def(Env* env_ptr) const {
  std::ostringstream oss;

  std::string type_str = CommonAttrInfo::get_inner_type_str(value_type_);
  if (middle_node_info_) {
    oss << "BS" << middle_node_info_->name() << "<" << type_str;
  } else {
    oss <<"BSFixedCommonInfo<" << type_str;
  }

  if (!middle_node_info_) {
    if (env_ptr->is_combine_feature() && !tool::is_item_field(get_bs_enum_str())) {
      oss << ", true";
    }
  }

  if (middle_node_info_) {
    oss << "> " << get_functor_name() << "{" << int_name_ << "}";
  } else {
    oss << "> " << get_functor_name() << "{" << tool::add_quote(prefix_adlog_) << ", " << int_name_ << "}";
  }

  return oss.str();
}

// BSHasFixedCommonInfoImpl<false> BSHasItemAdDspInfoCommonInfoAttr(no);
// 中间节点的 CommonInfo 单值比较特殊，需要带上类型
// BSHasLiveInfoCommonInfoImpl<int64_t> BSHasLiveInfoCommonInfoAttrNo{no};
std::string CommonInfoFixed::get_bs_scalar_exists_field_def(Env* env_ptr) const {
  std::ostringstream oss;

  std::string type_str = CommonAttrInfo::get_inner_type_str(value_type_);
  if (middle_node_info_) {
    std::string type_str = CommonAttrInfo::get_inner_type_str(value_type_);
    oss << "BSHas" << middle_node_info_->name() << common_info_camel_ << "Impl<" << type_str;
  } else {
    oss <<"BSHasFixedCommonInfoImpl<";
  }

  if (!middle_node_info_) {
    if (tool::is_adlog_user_field(prefix_)) {
      oss << "true";
    } else {
      oss << "false";
    }
  }

  oss << "> " << get_exists_functor_name();

  if (middle_node_info_) {
    oss << "{" << int_name_ << "}";
  } else {
    oss << "{" << tool::add_quote(prefix_adlog_) << ", " << int_name_ << "}";
  }

  return oss.str();
}

// BSFixedCommonInfo<BSRepeated<int64_t>> BSGetItemAdDspInfoCommonInfoAttr(no);
std::string CommonInfoFixed::get_bs_list_field_def(Env* env_ptr) const {
  std::ostringstream oss;

  std::string type_str = CommonAttrInfo::get_inner_type_str(value_type_);
  if (middle_node_info_) {
    oss << "BS" << middle_node_info_->name() << "<";
  } else {
    oss << "BSFixedCommonInfo<";
  }

  oss <<"BSRepeatedField<" << type_str << ">";
  if (!middle_node_info_) {
    if (env_ptr->is_combine_feature() && !tool::is_item_field(get_bs_enum_str())) {
      oss << ", true";
    }
  }

  oss << "> " << get_functor_name();
  if (middle_node_info_) {
    oss << "{" << int_name_ << "}";
  } else {
    oss << "{" << tool::add_quote(prefix_adlog_) << ", " << int_name_ << "}";
  }

  return oss.str();
}

std::string CommonInfoFixed::get_bs_map_field_def(Env* env_ptr) const {
  std::ostringstream oss;

  std::pair<std::string, std::string> map_type_str = CommonAttrInfo::get_map_inner_type_str(value_type_);
  if (middle_node_info_) {
    oss << "BS" << middle_node_info_->name() << "<";
  } else {
    oss << "BSFixedCommonInfo<";
  }

  oss <<"BSMapField<" << map_type_str.first << "," << map_type_str.second << ">";
  if (!middle_node_info_) {
    if (env_ptr->is_combine_feature() && !tool::is_item_field(get_bs_enum_str())) {
      oss << ", true";
    }
  }

  oss << "> " << get_functor_name();
  if (middle_node_info_) {
    oss << "{" << int_name_ << "}";
  } else {
    oss << "{" << tool::add_quote(prefix_adlog_) << ", " << int_name_ << "}";
  }

  return oss.str();
}

std::string CommonInfoFixed::get_bs_scalar_field_value_expr(Env* env_ptr) const {
  std::ostringstream oss;
  oss << get_functor_name() << "(bs, pos)";
  return oss.str();
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
