#pragma once

#include <glog/logging.h>
#include <nlohmann/json.hpp>
#include <absl/types/optional.h>

#include <algorithm>
#include <iostream>
#include <utility>
#include <list>
#include <mutex>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Declares clang::SyntaxOnlyAction.
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"

// Declares llvm::cl::extrahelp.
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

#include "clang/Lex/Preprocessor.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/Frontend/CompilerInstance.h"
#include "llvm/ADT/StringRef.h"

#include "Config.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

using nlohmann::json;
using llvm::dyn_cast;

class Env;

clang::SourceRange find_source_range(clang::Stmt* stmt);

std::string stmt_to_string(clang::Stmt* stmt, unsigned suppressTagKeyword = 0);

clang::Expr* get_first_caller(clang::Expr* initExpr, const Env* env_ptr = nullptr);

const clang::TemplateArgumentList* get_proto_map_args_list(clang::QualType qualType);

bool is_map_value_builtin(clang::QualType qualType);

bool is_check_item_pos(clang::IfStmt* ifStmt);

bool is_prefix(const std::string& s);

std::map<std::string, std::set<std::string>> collect_prefix(json d);

void collect_prefix_recursive(json d, std::set<std::string>* str_set);

json split_feature_def(json d,
                       const std::map<std::string, std::set<std::string>>& prefixs,
                       const std::map<std::string, int>& enum_map);

json split_feature_def_recursive(const json& d,
                                 std::string prefix,
                                 const std::map<std::string, int>& enum_map);

json replace_enum(const json& d,
                  const std::map<std::string, int>& enum_map);

bool is_basic_type(clang::QualType qual_type);

bool is_action_detail_find_expr(const std::string& adlog_expr);
bool is_action_detail_list_expr(const std::string& adlog_expr);
bool is_action_detail_find_expr(clang::Expr* expr, const Env& env);
bool is_action_detail_list_expr(clang::Expr* expr, const Env& env);

bool is_special_adlog_expr(const std::string& adlog_expr);
bool is_special_adlog_expr(clang::Expr* expr, const Env& env);

bool is_integer(const std::string & s);

bool starts_with(const std::string& s, const std::string& x);
bool ends_with(const std::string& s, const std::string& x);

std::string lower(const std::string& s);

absl::optional<int> find_common_attr_int_value(clang::Expr* expr);
absl::optional<int> find_common_attr_int_value_from_expr(clang::Expr* expr);
absl::optional<std::string> find_common_attr_enum_name_from_expr(clang::Expr* expr);

template<typename T>
std::string pointer_to_str(T* ptr) {
  std::ostringstream oss;
  oss << ptr;
  return oss.str();
}

bool ends_with_ignore_space(const std::string& s, char c);
bool is_only_space(const std::string& s);
bool is_only_semicolon(const std::string& s);
std::string fix_semicolon(const std::string& s);

namespace tool {

bool is_middle_node_expr(clang::Expr* expr, const Env& env);
bool is_middle_node_root(const std::string& s);

/// 中间节点根节点，如 GetLiveInfo(adlog.item(pos))
bool is_middle_node_expr_root(clang::Expr* expr, const Env& env);

bool is_common_info_vector(clang::QualType qual_type);

/// 待修复，需要更精确，目前逻辑和 is_common_info_struct 一样
bool is_common_info_enum(clang::QualType qual_type);

/// proto 中的 reptead common info, 目前的实现不是很准确，会将 attr.int_list_value() 也当做
/// repeated_common_info 节点, 需要再更精确些
bool is_repeated_common_info(clang::QualType qual_type);

bool is_repeated_common_info_size(const std::string& method_name);

bool is_common_info_struct(clang::QualType qual_type);

bool is_combine_feature(const std::string& feature_type);

bool is_user_feature(const std::string& feature_type);

bool is_item_feature(const std::string& feature_type);

bool is_sparse_feature(const std::string& feature_type);

bool is_dense_feature(const std::string& feature_type);

bool is_item_field(const std::string& s);

bool is_adlog_user_field(const std::string& s);

bool is_adlog_field(const std::string& s);

bool is_reco_user_field(const std::string& s);

bool is_item_type_enum(clang::QualType qual_type);

bool is_ad_callback_log_enum(clang::QualType qual_type);

bool is_ad_action_type_enum(clang::QualType qual_type);

bool is_ad_enum(clang::QualType qual_type);

bool is_basic_array(clang::QualType qual_type);

bool is_builtin_simple_type(clang::QualType qual_type);

std::string get_bs_scalar_exists_expr(Env* env_ptr,
                                      const std::string& common_info_prefix,
                                      const std::string& value_type);

bool is_int32_type(const std::string& type_str);

std::string get_builtin_type_str(clang::QualType qual_type);

bool is_action_detail_map(clang::QualType qual_type);

std::string get_exists_name(const std::string& name);

std::string fix_std_string(const std::string& s);

std::string fix_string_view(const std::string& s);

std::string get_bs_correspond_path(const std::string& filename);

std::string read_file_to_string(const std::string& filename);

bool is_bs_already_rewritten(const std::string& filename);

bool is_file_exists(const std::string& name);

std::string rm_continue_break(const std::string& s);

std::string find_last_include(const std::string& content);

bool is_string(const std::string& type_str);
bool is_string(clang::QualType qual_type);

bool is_char_arr(clang::QualType qual_type);

std::string add_quote(const std::string& s);

bool is_from_info_util(const std::string& s);

std::string fix_ad_enum(const std::string& s);

inline std::string bool_to_string(bool v) { return v ? "true" : "false"; }

std::string adlog_to_bs_enum_str(const std::string& s);

std::string dot_underscore_to_camel(const std::string& s);

bool is_var_proto_list(clang::QualType qualType);
bool is_var_proto_map(clang::QualType qualType);
bool is_var_proto_message(clang::QualType qual_type);

bool is_repeated_proto_message(clang::QualType qual_type);

std::vector<int> find_common_info_values_in_file(const std::string& filename);

bool is_skip(const std::string& feature_name);

bool is_enum_decl(clang::Expr* expr);

bool is_implicit_loop_var(const std::string& name);

bool is_from_implicit_loop_var(const std::string &s);

bool is_cxx_default_arg_expr(clang::Expr* expr);

std::string trim_this(const std::string& s);

bool is_int_vector(clang::QualType qual_type);

std::string rm_surround_big_parantheses(const std::string& s);
std::string add_surround_big_parantheses(const std::string& s);

bool is_action_info(const std::string& type_str);
bool is_action_info(clang::QualType qual_type);

bool is_repeated_proto(clang::QualType qual_type);
bool is_repeated_proto_iterator(clang::QualType qual_type);

bool is_map_proto(clang::QualType qual_type);
bool is_map_proto_iterator(clang::QualType qual_type);

bool is_repeated_action_info(clang::QualType qual_type);

std::string get_bs_type_str(clang::QualType qual_type, bool is_combine_user);

absl::optional<std::string> get_repeated_proto_inner_type(clang::QualType qual_type);
absl::optional<std::string> get_repeated_proto_iterator_inner_type(clang::QualType qual_type);

absl::optional<std::pair<std::string, std::string>> get_map_proto_inner_type(clang::QualType qual_type);

std::string get_bs_repeated_field_type(const std::string& type_str,
                                      bool is_combine_user);

std::string get_bs_map_field_type(const std::string& key_type_str,
                                  const std::string& value_type_str,
                                  bool is_combine_user);

bool is_repeated_proto_ptr(clang::QualType qual_type);

bool is_reco_proto(clang::QualType qual_type);

std::string replace_adlog_to_bslog(const std::string& s);

bool is_map_repeated_int_list_type(clang::QualType qual_type);

bool is_map_int_int_type(clang::QualType qual_type);

clang::Expr* get_inner_expr(clang::Expr* expr);

std::string rm_empty_line(const std::string& s);

std::string trim_tail_underscore(const std::string& s);

bool is_common_info_list_or_map_loop_stmt(clang::Stmt* stmt);

std::string trim_tail_size(const std::string& s);

bool contains_var(const std::string& expr_str, const std::string& var_name);

bool is_map_item_type_int_type(clang::QualType qual_type);

std::string get_last_type_str(const std::string& s);

bool has_cc_file(const std::string& filename);

bool is_pointer(clang::QualType qual_type);
bool is_proto_map_string_float(clang::QualType qual_type);
bool is_proto_map_string_float_ptr(clang::QualType qual_type);
bool is_proto_map_string_float_iter(clang::QualType qual_type);

std::string trim_exists(const std::string& s);

bool is_deref_implicit_loop_begin(const std::string s);

bool is_int_type(const std::string& type_str);
bool is_bool_type(const std::string& type_str);
bool is_float_type(const std::string& type_str);
bool is_repeated_proto_list_leaf_type(clang::QualType qual_type);

std::vector<int> get_int_list_values_from_init_str(const std::string& s);
std::string add_filename_suffix(const std::string& filename, const std::string& suffix);

bool is_str_from_reco_user_info(const std::string& bs_enum_str);

std::string replace_simple_text(const std::string &s);

std::string insert_str_at_block_begin(const std::string &s, const std::string &new_str);

std::string strip_suffix_semicolon_newline(const std::string& s);

// 找到 bs equal if 结尾。
size_t find_bs_equal_end(const std::string& s);

std::string insert_str_after_bs_equal_end(const std::string &s, const std::string &new_str);

}  // namespace tool

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
