#pragma once

#include <absl/types/optional.h>
#include <nlohmann/json.hpp>

#include <map>
#include <set>
#include <string>
#include <vector>
#include <unordered_map>

#include "../Type.h"
#include "CommonInfoMultiIntList.h"
#include "CommonInfoPrepare.h"
#include "ConstructorInfo.h"
#include "MethodInfo.h"
#include "NewVarDef.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/StmtCXX.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/Support/CommandLine.h"

#include "TemplateParamInfo.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

using nlohmann::json;

/// 类相关的信息，包括特征或者 item_filter、label_extractor
class FeatureInfo {
 public:
  FeatureInfo() = default;
  explicit FeatureInfo(const std::string& feature_name):
    feature_name_(feature_name),
    constructor_info_(feature_name) {}

  const std::string& feature_name() const { return feature_name_; }
  void set_feature_name(const std::string& feature_name) { feature_name_ = feature_name; }

  const std::string& feature_type() const { return feature_type_; }
  void set_feature_type(const std::string& feature_type) { feature_type_ = feature_type; }

  void set_origin_file(const std::string& filename) { origin_file_ = filename; }
  const std::string& origin_file() const { return origin_file_; }

  void set_is_template(bool filename) { is_template_ = filename; }
  bool is_template() const { return is_template_; }

  const ConstructorInfo& constructor_info() const { return (constructor_info_); }
  ConstructorInfo& mutable_constructor_info() { return (constructor_info_); }

  void set_extract_method_content(const std::string& extract_method_content) {
    extract_method_content_ = extract_method_content;
  }
  const std::string& extract_method_content() const { return extract_method_content_; }
  std::string& mutable_extract_method_content() { return extract_method_content_; }

  void set_header_content(const std::string& header_content) { header_content_ = header_content; }
  const std::string& header_content() const { return header_content_; }

  void add_field_decl(const clang::FieldDecl* stmt) { field_decls_.push_back(stmt); }
  const std::vector<const clang::FieldDecl*>& field_decls() const { return field_decls_; }

  absl::optional<clang::SourceLocation> last_field_decl_end_log() const;

  void set_file_id(const clang::FileID& file_id) { file_id_ = file_id; }
  const clang::FileID& file_id() const { return file_id_; }

  void add_field_def(const std::string& bs_enum_str,
                     const std::string& name,
                     const std::string& new_def,
                     NewVarType new_var_type,
                     AdlogVarType adlog_var_type);
  void add_field_def(const std::string& bs_enum_str,
                     const std::string& name,
                     const std::string& new_def,
                     NewVarType new_var_type,
                     ExprType expr_type,
                     AdlogVarType adlog_var_type);
  void add_field_def(const std::string& bs_enum_str,
                     const std::string& name,
                     const std::string& new_def,
                     const std::string& exists_name,
                     const std::string& new_exists_def,
                     NewVarType new_var_type,
                     AdlogVarType adlog_var_type);
  const std::unordered_map<std::string, NewVarDef>& new_field_defs() const { return new_field_defs_; }

  void set_middle_node_info(const std::string& bs_enum_str,
                            const std::string& middle_node_root,
                            const std::string& middle_node_field);

  void set_common_info_prefix_name_value(const std::string& bs_enum_str,
                                         const std::string& prefix_adlog,
                                         const std::string& name_value_alias);

  void set_action_var_name(const std::string& bs_enum_str, const std::string& action_var_name);

  /// OverviewHandler 后执行
  void clear_new_field_defs() { new_field_defs_.clear(); }

  void add_binary_op_stmt(clang::BinaryOperator* binary_operator) {
    binary_op_stmts_.push_back(binary_operator);
  }

  const std::vector<clang::BinaryOperator*>& binary_op_stmts() const { return binary_op_stmts_; }

  clang::BinaryOperator* find_update_action_stmt() const;

  void set_action(const std::string& action) { action_.emplace(action); }

  void add_template_common_info_value(int value) { template_common_info_values_.insert(value); }
  const std::set<int>& template_common_info_values() const { return template_common_info_values_; }

  bool is_member(clang::Expr* expr) const;
  bool is_member(const std::string& name) const;
  bool is_int_list_member(const std::string& name) const;
  bool is_int_list_member(clang::Expr* expr) const;
  bool is_int_list_member(const std::string& name, clang::QualType qual_type) const;

  bool is_common_info_enum_member(const std::string& name, clang::QualType qual_type) const;

  void add_int_list_member_single_value(const std::string& name, int value);
  void add_int_list_member_values(const std::string& name, const std::vector<int>& values);

  const std::vector<int>& get_int_list_member_values(const std::string& name) const;

  const std::string& origin_buffer() const { return origin_buffer_; }

  void set_origin_buffer(const std::string& origin_buffer) { origin_buffer_ = origin_buffer; }

  void add_other_method(const std::string& name,
                        clang::QualType return_type,
                        const std::string& bs_return_type,
                        const std::string& decl,
                        const std::string& body);
  const std::unordered_map<std::string, MethodInfo>& other_methods() const { return other_methods_; }

  MethodInfo& touch_method_info(const std::string& method_name);
  const MethodInfo* find_method_info(const std::string& method_name) const;
  MethodInfo* mutable_method_info(const std::string& method_name);

  bool is_feature_other_method(const std::string& method_name) const;

  /// 用于区分 combine 特征里的 user 字段。
  /// 由于 item 特征里也有写错的，会访问 user 字段，因此 item 特征也被认为是 combine。
  bool is_combine() const;

  absl::optional<CommonInfoMultiIntList>& touch_common_info_multi_int_list(const std::string& prefix);
  absl::optional<CommonInfoMultiIntList>& mutable_common_info_multi_int_list() {
    return (common_info_multi_int_list_);
  }

  const absl::optional<CommonInfoMultiIntList>& common_info_multi_int_list() const {
    return (common_info_multi_int_list_);
  }

  absl::optional<CommonInfoPrepare>& mutable_common_info_prepare() { return (common_info_prepare_); }
  const absl::optional<CommonInfoPrepare>& common_info_prepare() const { return (common_info_prepare_); }

  const std::vector<std::string>& template_var_names() const { return (template_var_names_); }
  void add_template_var_name(const std::string& var_name) { template_var_names_.push_back(var_name); }

  const json& output() const { return (output_); }
  json& mutable_output() { return (output_); }

  /// 从 constructor_info 以及 field_def 里获取字段信息保存到 output_ 中。
  void gen_output();

  bool has_hash_fn_str() const { return has_hash_fn_str_; }
  void set_has_hash_fn_str(bool v) { has_hash_fn_str_ = v; }

  const std::unordered_map<std::string, NewVarType>& bs_enum_var_type() const { return (bs_enum_var_type_); }
  void add_bs_enum_var_type(const std::string& bs_enum_str, NewVarType new_var_type) {
    bs_enum_var_type_.insert({bs_enum_str, new_var_type});
  }

  bool is_in_bs_enum_var_type(const std::string& bs_enum_str) const {
    return bs_enum_var_type_.find(bs_enum_str) != bs_enum_var_type_.end();
  }

  bool has_cc_file() const { return has_cc_file_; }
  void set_has_cc_file(bool v) { has_cc_file_ = v; }

  bool has_query_token() const { return has_query_token_; }
  void set_has_query_token(bool v) { has_query_token_ = v; }

  bool has_common_info_multi_int_list() const { return has_common_info_multi_int_list_; }
  void set_has_common_info_multi_int_list(bool v) { has_common_info_multi_int_list_ = v; }

  bool has_common_info_multi_map() const {return has_common_info_multi_map_;}
  void set_has_common_info_multi_map(bool v) {has_common_info_multi_map_ = v;}

  const std::unordered_map<std::string, NewVarDef>& middle_node_bs_enum_var_type() const {
    return (middle_node_bs_enum_var_type_);
  }

  /// scalar
  void add_middle_node_bs_enum_var_type(const std::string& middle_node_bs_enum_str,
                                        NewVarType new_var_type,
                                        const std::string& adlog_field);
  /// list
  void add_middle_node_bs_enum_var_type(const std::string& middle_node_bs_enum_str,
                                        NewVarType new_var_type,
                                        const std::string& adlog_field,
                                        const std::string& list_inner_type);
  bool is_in_middle_node_bs_enum_var_type(const std::string& middle_node_bs_enum_str) const {
    return middle_node_bs_enum_var_type_.find(middle_node_bs_enum_str) != middle_node_bs_enum_var_type_.end();
  }

  void add_specialization_class(const std::string& name);

  void add_specialization_param_value(const std::string& name,
                                      size_t index,
                                      const std::string& param_value);

  void add_specialization_param_value(const std::string& name,
                                      size_t index,
                                      const std::string& param_value,
                                      int enum_value);

  TemplateParamInfo* touch_template_param_ptr(const std::string& name, size_t index);

  const std::unordered_map<std::string, std::vector<TemplateParamInfo>>& specialization_class_names() const {
    return (specialization_class_names_);
  }

  void set_template_param_names(const std::vector<std::string>& param_names) {
    template_param_names_ = param_names;
  }

  const absl::optional<std::string>& reco_extract_body() const { return reco_extract_body_; }
  void set_reco_extract_body(const std::string& body) { reco_extract_body_.emplace(body); }

  const absl::optional<std::string>& cc_filename() const { return cc_filename_; }
  void set_cc_filename(const std::string& filename) { cc_filename_.emplace(filename); }

 private:
  std::string feature_name_;
  std::string feature_type_;
  std::string origin_file_;

  absl::optional<std::string> cc_filename_;

  bool is_template_ = false;

  /// specialization_name -> [TemplateParamInfo, ...]
  std::unordered_map<std::string, std::vector<TemplateParamInfo>> specialization_class_names_;

  std::vector<std::string> template_param_names_;

  clang::FileID file_id_;

  ConstructorInfo constructor_info_;
  std::string extract_method_content_;
  std::string header_content_;

  /// 模板类对应的字段
  std::unordered_map<std::string, NewVarDef> new_field_defs_;
  std::vector<clang::BinaryOperator*> binary_op_stmts_;
  absl::optional<std::string> action_;

  /// 模板参数中出现的 common info value
  std::set<int> template_common_info_values_;

  /// OverviewHandler 中收集
  std::vector<std::string> template_var_names_;

  std::vector<const clang::FieldDecl*> field_decls_;
  std::unordered_map<std::string, std::vector<int>> int_list_member_values_;

  std::string origin_buffer_;
  std::unordered_map<std::string, MethodInfo> other_methods_;

  absl::optional<CommonInfoMultiIntList> common_info_multi_int_list_;
  absl::optional<CommonInfoPrepare> common_info_prepare_;

  json output_;
  bool has_hash_fn_str_ = false;

  // 普通叶子节点的 list 对应的 bs_enum, 不包括 action detail, common info 等。
  // 用于判断 for 循环中的 size method 是否是叶子节点。
  std::unordered_map<std::string, NewVarType> bs_enum_var_type_;
  // 中间节点的叶子节点 list 或者对应的 bs_enum。不以 adlog 开头。需要保存 adlog_field, type,
  // list_inner_type 等信息。
  // 单独存一个 map 和普通节点区分开。
  std::unordered_map<std::string, NewVarDef> middle_node_bs_enum_var_type_;

  bool has_cc_file_ = false;
  bool has_query_token_ = false;
  bool has_common_info_multi_int_list_ = false;
  bool has_common_info_multi_map_ = false;

  /// reco user info
  absl::optional<std::string> reco_extract_body_;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
