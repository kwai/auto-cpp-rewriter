#include <regex>
#include <string>

#include "absl/strings/str_split.h"

#include "../Tool.h"
#include "../Env.h"
#include "CommonInfoLeaf.h"
#include "../handler/StrictRewriter.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

void CommonInfoLeaf::update_method_name(const std::string& method_name) {
  if (!is_ready_) {
    CommonInfoCore::update_method_name(method_name);
    is_ready_ = true;
  }
}

void CommonInfoLeaf::update_size_method_name(const std::string& size_method_name) {
  if (!is_ready_) {
    CommonInfoCore::update_size_method_name(size_method_name);
    is_ready_ = true;
  }
}

std::string CommonInfoLeaf::get_exists_expr(Env* env_ptr) const {
  if (method_name_.size() == 0) {
    LOG(INFO) << "cannot find common info method, prefix: " << prefix_
              << ", address: " << this;
    return "";
  }

  std::string inner_type_str = CommonAttrInfo::get_inner_type_str(value_type_);
  if (inner_type_str.size() == 0) {
    LOG(INFO) << "cannot get inner_type_str, prefix: " << prefix_;
  }

  std::ostringstream oss;
  std::string bs_enum_str = get_bs_enum_str();

  if (const absl::optional<NewVarDef>& var = env_ptr->find_new_def(bs_enum_str)) {
    if (is_scalar()) {
      oss << var->exists_name();
    } else {
      if (var->name().size() == 0) {
        LOG(INFO) << "cannot find common info var_name, prefix: " << prefix_;
        return "";
      }

      oss << "!" << var->name() << ".is_empty()";
    }
  }

  return oss.str();
}

std::string CommonInfoLeaf::get_bs_scalar_def(Env* env_ptr) const {
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

std::string CommonInfoLeaf::get_bs_scalar_exists_def(Env* env_ptr) const {
  if (method_name_.size() == 0) {
    LOG(INFO) << "common info fixed missing method_name!";
    return "";
  }

  if (env_ptr == nullptr) {
    LOG(INFO) << "env_ptr is nullptr!";
    return "";
  }

  std::string bs_enum_str = get_bs_enum_str();
  std::string new_name = tool::get_exists_name(env_ptr->find_valid_new_name(bs_enum_str));
  std::string type_str = CommonAttrInfo::get_inner_type_str(value_type_);

  std::ostringstream oss;
  oss << "bool " << new_name << " = " << get_exists_functor_name() << "(bs, pos)";
  return oss.str();
}

std::string CommonInfoLeaf::get_bs_list_def(Env* env_ptr) const {
  if (method_name_.size() == 0) {
    LOG(INFO) << "common info fixed missing method_name!" << ", address: " << this;
    return "";
  }

  if (env_ptr == nullptr) {
    LOG(INFO) << "env_ptr is nullptr!";
    return "";
  }

  std::string bs_enum_str = get_bs_enum_str();
  std::string new_name = env_ptr->find_valid_new_name(bs_enum_str);
  std::string type_str = get_list_inner_type_str(value_type_);
  LOG(INFO) << "get_bs_list_def, type_str: " << type_str;

  std::ostringstream oss;
  oss << "BSRepeatedField<" << type_str;
  if (env_ptr->is_combine_feature() && !is_item_field(bs_enum_str)) {
    oss << ", true";
  }
  oss << "> " << new_name << " = std::move(" << get_functor_name() << "(bs, pos))";

  return oss.str();
}

std::string CommonInfoLeaf::get_bs_map_def(Env* env_ptr) const {
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
  if (env_ptr->is_combine_feature() && !is_item_field(bs_enum_str)) {
    oss << ", true";
  }
  oss << "> " << new_name << " = std::move(" << get_functor_name() << "(bs, pos))";

  return oss.str();
}

std::string CommonInfoLeaf::get_bs_var_name(Env* env_ptr) const {
  std::string bs_enum_str = get_bs_enum_str();

  if (const absl::optional<NewVarDef>& var = env_ptr->find_new_def(bs_enum_str)) {
    if (is_list()) {
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
    } else if (is_scalar()) {
      return var->name();
    } else {
      // map 直接根据 member_expr 替换，不会调用到这个函数。
      return var->name();
    }
  } else {
    LOG(INFO) << "cannot find list or map var_name in env, bs_enum_str: " << bs_enum_str;
    return "";
  }
}

std::string CommonInfoLeaf::get_bs_var_def_name(Env* env_ptr) const {
  std::string bs_enum_str = get_bs_enum_str();

  if (const absl::optional<NewVarDef> &var = env_ptr->find_new_def(bs_enum_str)) {
    return var->name();
  }

  LOG(INFO) << "cannot find list or map var_name in env, bs_enum_str: "
            << bs_enum_str;
  return "";
}

std::string CommonInfoLeaf::bs_enum_str_to_camel(const std::string bs_enum_str) const {
  std::ostringstream oss;

  std::vector<std::string> arr = absl::StrSplit(bs_enum_str, "_");
  for (const std::string& s: arr) {
    if (starts_with(s, "adlog") || s == "exists" || s.size() == 0) {
      continue;
    }

    oss << char(toupper(s[0])) << s.substr(1);
  }

  return oss.str();
}

std::string CommonInfoLeaf::get_functor_name() const {
  std::ostringstream oss;

  std::string bs_enum_str = get_bs_enum_str();
  oss << "BSGet" << bs_enum_str_to_camel(bs_enum_str);

  return oss.str();
}

std::string CommonInfoLeaf::get_exists_functor_name() const {
  std::string functor_name = get_functor_name();
  static std::regex p("BSGet");
  return std::regex_replace(functor_name, p, "BSHas");
}

// BSFixedCommonInfo<int64_t> BSGetItemAdDspInfoCommonInfoAttr(no);
std::string CommonInfoLeaf::get_bs_scalar_field_def(Env* env_ptr) const {
  std::ostringstream oss;

  std::string bs_enum_str = get_bs_enum_str();
  std::string type_str = CommonAttrInfo::get_inner_type_str(value_type_);
  oss << get_functor_tmpl() << "<" << type_str;
  if (env_ptr->is_combine_feature() && !is_item_field(bs_enum_str)) {
    oss << ", true";
  }
  oss << "> " << get_functor_name() << "{" << get_field_def_params() << "}";

  return oss.str();
}

// BSHasLiveInfoImpl BSHasLiveInfoCommonInfoAttrKey2332;
std::string CommonInfoLeaf::get_bs_scalar_exists_field_def(Env* env_ptr) const {
  std::ostringstream oss;

  oss << get_exists_functor_tmpl() << " ";
  oss << get_exists_functor_name()
      << "{" << get_field_def_params() << "}";

  return oss.str();
}

// BSFixedCommonInfo<BSRepeated<int64_t>> BSGetItemAdDspInfoCommonInfoAttr(no);
std::string CommonInfoLeaf::get_bs_list_field_def(Env* env_ptr) const {
  std::ostringstream oss;

  std::string bs_enum_str = get_bs_enum_str();
  std::string type_str = get_list_inner_type_str(value_type_);
  oss << get_functor_tmpl() << "<BSRepeatedField<" << type_str << ">";
  if (env_ptr->is_combine_feature() && !is_item_field(bs_enum_str)) {
    oss << ", true";
  }
  oss << "> " << get_functor_name() << "{" << get_field_def_params() << "}";

  return oss.str();
}

std::string CommonInfoLeaf::get_bs_map_field_def(Env* env_ptr) const {
  std::ostringstream oss;

  std::string bs_enum_str = get_bs_enum_str();
  std::pair<std::string, std::string> map_type_str = CommonAttrInfo::get_map_inner_type_str(value_type_);
  oss << get_functor_tmpl() << "<BSMapField<" << map_type_str.first << "," << map_type_str.second << ">";
  if (env_ptr->is_combine_feature() && !is_item_field(bs_enum_str)) {
    oss << ", true";
  }
  oss << "> " << get_functor_name() << "{" << get_field_def_params() << "}";

  return oss.str();
}

void CommonInfoLeaf::copy_except_int_value(CommonInfoLeaf* common_info_leaf) {
  if (common_info_leaf == nullptr) {
    return;
  }

  // pre_text 中可能有包含 key_xxx 的参数。需要替换。
  // 如 userAttr 会被替换为 key_xxx。
  // 示例: teams/ad/ad_algorithm/feature/fast/impl/extract_user_ad_impression_realtime_new_extend.h
  // case ::auto_cpp_rewriter::CommonInfoAttr_NameExtendOne_ADLOGFULL_EVENT_NEXTDAY_STAY_INDUSTRY_ID_V3_LIST:
  //   helper(FeaturePrefix::COMBINE_USER_SIM_REALTIME_CONV_THIRD_INDUSTRY_NAME, userAttr, result);
  //   break;
  bs_pre_text_ = replace_value_key(common_info_leaf->get_bs_pre_text(),
                                   common_info_leaf->common_info_value());
  bs_loop_text_.emplace(replace_value_key(common_info_leaf->get_bs_loop_text(),
                                          common_info_leaf->common_info_value()));
  bs_post_text_.emplace(replace_value_key(common_info_leaf->get_bs_post_text(),
                                          common_info_leaf->common_info_value()));

  list_loop_var_ = common_info_leaf->list_loop_var();
  name_value_alias_ = common_info_leaf->name_value_alias();
  common_info_type_ = common_info_leaf->common_info_type();

  // core
  method_name_ = common_info_leaf->method_name();
  size_method_name_ = common_info_leaf->size_method_name();
  value_type_ = common_info_leaf->value_type();
  is_ready_ = common_info_leaf->is_ready();
  is_for_stmt_ = common_info_leaf->is_for_stmt();
  is_size_method_in_loop_init_ = common_info_leaf->is_size_method_in_loop_init();
  has_list_method_address_ = common_info_leaf->has_list_method_address();
}

std::string CommonInfoLeaf::replace_value_key(const std::string& s, int common_info_value) const {
  std::regex p("key_" + std::to_string(common_info_value) + "([^\\d\\w_])?");
  std::string text = std::string("key_") + std::to_string(common_info_value_) + "$1";
  return std::regex_replace(s, p, text);
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
