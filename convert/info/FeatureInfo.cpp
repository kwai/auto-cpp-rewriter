#include <absl/types/optional.h>
#include <regex>

#include "MiddleNodeInfo.h"
#include "NewVarDef.h"
#include "TemplateParamInfo.h"
#include "clang/AST/Decl.h"

#include <absl/strings/str_split.h>
#include "../Tool.h"
#include "FeatureInfo.h"
#include "MethodInfo.h"
#include "AdlogFieldInfo.h"
#include "MiddleNodeInfo.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

absl::optional<clang::SourceLocation> FeatureInfo::last_field_decl_end_log() const {
  if (field_decls_.size() == 0) {
    return absl::nullopt;
  }

  return absl::make_optional(field_decls_.back()->getEndLoc());
}

void FeatureInfo::add_field_def(const std::string& bs_enum_str,
                                const std::string& name,
                                const std::string& new_def,
                                NewVarType new_var_type,
                                AdlogVarType adlog_var_type) {
  NewVarDef new_var_def(bs_enum_str, name, new_def, new_var_type, adlog_var_type);
  new_field_defs_.emplace(bs_enum_str, new_var_def);
}

void FeatureInfo::add_field_def(const std::string& bs_enum_str,
                                const std::string& name,
                                const std::string& new_def,
                                NewVarType new_var_type,
                                ExprType expr_type,
                                AdlogVarType adlog_var_type) {
  NewVarDef new_var_def(bs_enum_str, name, new_def, new_var_type, adlog_var_type);
  new_var_def.set_expr_type(expr_type);
  new_field_defs_.emplace(bs_enum_str, new_var_def);
}

void FeatureInfo::add_field_def(const std::string& bs_enum_str,
                                const std::string& name,
                                const std::string& new_def,
                                const std::string& exists_name,
                                const std::string& new_exists_def,
                                NewVarType new_var_type,
                                AdlogVarType adlog_var_type) {
  NewVarDef new_var_def(bs_enum_str, name, new_def, new_var_type, adlog_var_type);
  new_var_def.set_exists_var_def(exists_name, new_exists_def);
  new_field_defs_.emplace(bs_enum_str, new_var_def);
}

void FeatureInfo::set_middle_node_info(const std::string& bs_enum_str,
                                       const std::string& middle_node_root,
                                       const std::string& middle_node_field) {
  auto it = new_field_defs_.find(bs_enum_str);
  if (it == new_field_defs_.end()) {
    LOG(INFO) << "cannot find new field in new_field_defs, bs_enum_str: " << bs_enum_str
              << ", middle_node_root: " << middle_node_root
              << ", middle_node_field: " << middle_node_field;
    return;
  }

  it->second.set_middle_node_root(middle_node_root);
  it->second.set_middle_node_field(middle_node_field);
}

void FeatureInfo::set_common_info_prefix_name_value(const std::string& bs_enum_str,
                                                    const std::string& prefix_adlog,
                                                    const std::string& name_value_alias) {
  auto it = new_field_defs_.find(bs_enum_str);
  if (it == new_field_defs_.end()) {
    LOG(INFO) << "cannot find new field in new_field_defs, bs_enum_str: " << bs_enum_str
              << ", prefix_adlog: " << prefix_adlog
              << ", name_value_alias: " << name_value_alias;
    return;
  }

  it->second.set_common_info_prefix_adlog(prefix_adlog);
  it->second.set_common_info_name_value_alias(name_value_alias);
}

void FeatureInfo::set_action_var_name(const std::string& bs_enum_str,
                                      const std::string& action_var_name) {
  auto it = new_field_defs_.find(bs_enum_str);
  if (it == new_field_defs_.end()) {
    LOG(INFO) << "cannot find new field in new_field_defs, bs_enum_str: " << bs_enum_str
              << ", action_var_name: " << action_var_name;
    return;
  }

  it->second.set_action_var_name(action_var_name);
}

clang::BinaryOperator* FeatureInfo::find_update_action_stmt() const {
  if (!action_) {
    return nullptr;
  }

  for (size_t i = 0; i < binary_op_stmts_.size(); i++) {
    std::vector<std::string> arr = absl::StrSplit(stmt_to_string(binary_op_stmts_[i]), "=");
    if (arr.size() == 2) {
      std::string s = std::regex_replace(arr[0], std::regex(" "), "");
      if (s == *action_) {
        return binary_op_stmts_[i];
      }
    }
  }

  return nullptr;
}

bool FeatureInfo::is_member(clang::Expr* expr) const {
  std::string expr_str = tool::trim_this(stmt_to_string(expr));
  for (size_t i = 0; i < field_decls_.size(); i++) {
    if (expr_str == field_decls_[i]->getNameAsString()) {
      return true;
    }
  }

  return false;
}

bool FeatureInfo::is_member(const std::string& name) const {
  for (size_t i = 0; i < field_decls_.size(); i++) {
    if (name == field_decls_[i]->getNameAsString()) {
      return true;
    }
  }

  return false;
}

bool FeatureInfo::is_int_list_member(const std::string &name) const {
  for (size_t i = 0; i < field_decls_.size(); i++) {
    if (field_decls_[i]->getNameAsString() == name) {
      if (tool::is_int_vector(field_decls_[i]->getType())) {
        return true;
      }
    }
  }

  return false;
}

bool FeatureInfo::is_int_list_member(clang::Expr* expr) const {
  return is_member(expr) && tool::is_int_vector(expr->getType());
}

bool FeatureInfo::is_int_list_member(const std::string& name, clang::QualType qual_type) const {
  return is_member(name) && tool::is_int_vector(qual_type);
}

bool FeatureInfo::is_common_info_enum_member(const std::string& name, clang::QualType qual_type) const {
  return is_member(name) && tool::is_common_info_enum(qual_type);
}

void FeatureInfo::add_int_list_member_single_value(const std::string& name, int value) {
  int_list_member_values_[name].push_back(value);
}

void FeatureInfo::add_int_list_member_values(const std::string& name, const std::vector<int>& values) {
  int_list_member_values_[name] = values;
}

const std::vector<int>& FeatureInfo::get_int_list_member_values(const std::string& name) const {
  auto it = int_list_member_values_.find(name);
  if (it != int_list_member_values_.end()) {
    return it->second;
  }

  static std::vector<int> empty;
  return (empty);
}

void FeatureInfo::add_other_method(const std::string& name,
                                   clang::QualType return_type,
                                   const std::string& bs_return_type,
                                   const std::string& decl,
                                   const std::string& body) {
  MethodInfo& method_info = touch_method_info(name);
  method_info.update(return_type, bs_return_type, decl, body);
}

MethodInfo& FeatureInfo::touch_method_info(const std::string& method_name) {
  auto it = other_methods_.find(method_name);
  if (it != other_methods_.end()) {
    return it->second;
  } else {
    return other_methods_.emplace(method_name, MethodInfo(method_name)).first->second;
  }
}

const MethodInfo* FeatureInfo::find_method_info(const std::string& method_name) const {
  auto it = other_methods_.find(method_name);
  if (it == other_methods_.end()) {
    return nullptr;
  }

  return &(it->second);
}

MethodInfo* FeatureInfo::mutable_method_info(const std::string& method_name) {
  const MethodInfo* method_info_ptr = find_method_info(method_name);
  return const_cast<MethodInfo*>(method_info_ptr);
}

bool FeatureInfo::is_feature_other_method(const std::string& method_name) const {
  return other_methods_.find(method_name) != other_methods_.end();
}

bool FeatureInfo::is_combine() const {
  return tool::is_combine_feature(feature_type_) || tool::is_item_feature(feature_type_);
}

absl::optional<CommonInfoMultiIntList>& FeatureInfo::touch_common_info_multi_int_list(const std::string& prefix) {
  if (!common_info_multi_int_list_) {
    common_info_multi_int_list_.emplace(prefix);
  }
  return (common_info_multi_int_list_);
}

void FeatureInfo::gen_output() {
  output_ = {
  {"normal_field", json::array()},
  {"middle_node", json::array()},
  {"norm_query", json::array()},
  {"field_def", json::array()},
  {"exists_field_def", json::array()},
  {"is_template", is_template_},
  {"specialization_class_names", json::object()},
  {"all_field", json::array()}};

  output_["h_file"] = origin_file_;
  output_["cc_file"] = "";

  if (is_template_) {
    for (auto it_name = specialization_class_names_.begin();
         it_name != specialization_class_names_.end();
         it_name++) {
      // [param, param1, ...]
      json params = json::array();

      for (size_t i = 0; i < it_name->second.size(); i++) {
        json arg = json::object();

        if (i < template_param_names_.size()) {
          const auto& param_info = it_name->second[i];
          arg["name"] = template_param_names_[i];
          arg["value_str"] = param_info.value_str();
          if (param_info.enum_value()) {
            arg["enum_value"] = *(param_info.enum_value());
          }
          if (param_info.qual_type()) {
            arg["type_str"] = param_info.qual_type()->getAsString();
          }
        } else {
          LOG(INFO) << "out of range, i: " << i
                    << ", template_param_names_.size(): " << template_param_names_.size();
        }

        params.push_back(std::move(arg));
      }

      output_["specialization_class_names"][it_name->first] = std::move(params);
    }
  }

  auto& constructor_info = mutable_constructor_info();
  constructor_info.fix_common_info_enums();

  const auto& adlog_field_infos = constructor_info.adlog_field_infos();

  for (const std::string &bs_field_enum : constructor_info.bs_field_enums()) {
    output_["normal_field"].emplace_back(json::object({{"name", bs_field_enum}}));
    auto it_field = adlog_field_infos.find(bs_field_enum);
    if (it_field != adlog_field_infos.end()) {
      const auto& field_info = it_field->second;
      auto& last = output_["normal_field"].back();
      last["adlog_field"] = field_info.adlog_field();
      last["adlog_field_type"] = "normal";

      if (field_info.adlog_field_type() == AdlogFieldType::COMMON_INFO) {
        static std::regex p("key:\\d+$");
        if (const auto& enum_name = field_info.common_info_enum_name()) {
          last["adlog_field_with_enum_name"] =
            std::regex_replace(last["adlog_field"].get<std::string>(), p, *enum_name);
          last["common_info_enum_name"] = *enum_name;
        } else {
          LOG(INFO) << "missing common_info_enum_name, bs_enum_str: " << bs_field_enum;
        }

        if (const auto& enum_value = field_info.common_info_enum_value()) {
          last["common_info_enum_value"] = *enum_value;
        } else {
          LOG(INFO) << "missing common_info_enum_value, bs_enum_str: " << bs_field_enum;
        }
      }
    }
  }

  for (const auto &common_info : constructor_info.common_info_enums()) {
    std::string bs_enum_str = common_info.bs_enum_str();
    if (bs_enum_str.size() > 0) {
      output_["normal_field"].emplace_back(json::object({{"name", bs_enum_str}}));
    }
  }

  for (const std::string &leaf : constructor_info.middle_node_leafs()) {
    output_["middle_node"].emplace_back(json::object({{"name", leaf}, {"adlog_field_type", "middle_node"}}));
  }

  if (constructor_info.has_get_norm_query()) {
    output_["norm_query"].emplace_back(json::object({{"name", "NormQuery"}}));
  }

  for (auto it = new_field_defs_.begin(); it != new_field_defs_.end(); it++) {
    json detail = json::object();

    detail["adlog_var_type"] = static_cast<int>(it->second.adlog_var_type());
    if (const auto& middle_node_root = it->second.middle_node_root()) {
      detail["middle_node_root"] = *middle_node_root;
    }
    if (const auto& middle_node_field = it->second.middle_node_field()) {
      detail["middle_node_field"] = *middle_node_field;
    }
    if (const auto& common_info_prefix_adlog = it->second.common_info_prefix_adlog()) {
      detail["common_info_prefix_adlog"] = *common_info_prefix_adlog;
    }
    if (const auto& common_info_name_value_alias = it->second.common_info_name_value_alias()) {
      detail["common_info_name_value_alias"] = *common_info_name_value_alias;
    }
    if (const auto& action_var_name = it->second.action_var_name()) {
      detail["action_var_name"] = *action_var_name;
    }

    if (tool::is_from_info_util(it->second.name())) {
      detail["name"] = it->second.name();
      output_["field_def"].push_back(detail);
    }

    if (tool::is_from_info_util(it->second.exists_name())) {
      detail["name"] = it->second.exists_name();
      output_["exists_field_def"].push_back(detail);
    }
  }

  // 统一写到 all_field 中，保存 bs_field_enum 和 adlog_field, 中间节点都展开
  for (size_t i = 0; i < output_["normal_field"].size(); i++) {
    output_["all_field"].push_back(output_["normal_field"][i]);
  }

  for (size_t i = 0; i < output_["field_def"].size(); i++) {
    if (output_["field_def"][i].contains("middle_node_root")) {
      std::string middle_node_root = output_["field_def"][i]["middle_node_root"].get<std::string>();
      const std::vector<std::string>& prefixs = MiddleNodeInfo::get_possible_adlog_prefix(middle_node_root);
      for (size_t j = 0; j < prefixs.size(); j++) {
        std::string middle_node_field = output_["field_def"][i]["middle_node_field"].get<std::string>();
        std::string leaf = prefixs[j] + std::string(".") + middle_node_field;
        std::string bs_field_enum = tool::adlog_to_bs_enum_str(leaf);
        json middle_node_leaf = json::object({{"name", leaf},
                                              {"bs_field_enum", bs_field_enum},
                                              {"middle_node_field", middle_node_field},
                                              {"adlog_field_type", "middle_node"}});
        output_["all_field"].emplace_back(middle_node_leaf);
      }
    }
  }
}

void FeatureInfo::add_middle_node_bs_enum_var_type(const std::string& middle_node_bs_enum_str,
                                                   NewVarType new_var_type,
                                                   const std::string& adlog_field) {
  NewVarDef new_var_def;
  new_var_def.set_bs_enum_str(middle_node_bs_enum_str);
  new_var_def.set_new_var_type(new_var_type);
  new_var_def.set_adlog_field(adlog_field);

  middle_node_bs_enum_var_type_.insert({middle_node_bs_enum_str, new_var_def});
}

void FeatureInfo::add_middle_node_bs_enum_var_type(const std::string& middle_node_bs_enum_str,
                                                   NewVarType new_var_type,
                                                   const std::string& adlog_field,
                                                   const std::string& list_inner_type) {
  NewVarDef new_var_def;
  new_var_def.set_bs_enum_str(middle_node_bs_enum_str);
  new_var_def.set_new_var_type(new_var_type);
  new_var_def.set_adlog_field(adlog_field);
  new_var_def.set_list_inner_type(list_inner_type);

  middle_node_bs_enum_var_type_.insert({middle_node_bs_enum_str, new_var_def});
}

void FeatureInfo::add_specialization_class(const std::string& name) {
  specialization_class_names_[name] = {};
}

void FeatureInfo::add_specialization_param_value(const std::string& name,
                                                 size_t index,
                                                 const std::string& param_value) {
  if (index >= specialization_class_names_[name].size()) {
    specialization_class_names_[name].resize(index + 1);
  }

  specialization_class_names_[name][index].set_value_str(param_value);
}

void FeatureInfo::add_specialization_param_value(const std::string& name,
                                                 size_t index,
                                                 const std::string& param_value,
                                                 int enum_value) {
  if (index >= specialization_class_names_[name].size()) {
    specialization_class_names_[name].resize(index + 1);
  }

  specialization_class_names_[name][index].set_value(param_value, enum_value);
}

TemplateParamInfo* FeatureInfo::touch_template_param_ptr(const std::string& name, size_t index) {
  auto it = specialization_class_names_.find(name);
  if (it == specialization_class_names_.end()) {
    specialization_class_names_[name] = {};
  }

  auto& params = specialization_class_names_[name];
  if (index >= params.size()) {
    params.resize(index + 1);
  }

  return &(params[index]);
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
