#pragma once

#include <absl/strings/str_join.h>
#include <absl/types/optional.h>

#include <map>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>

#include "../Type.h"
#include "VarDeclInfo.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Expr.h"
#include "clang/AST/StmtCXX.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "AdlogFieldInfo.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

/// 构造函数中用到的 common info enum, 如果是在 if 中出现的需要保存 if 结束的位置, 用于插入 attr_meta。
/// 必须在解析 Extract 函数时遇到具体的 common info 变量，才知道完整的 bs_field_enum, 因此替换是在
/// AdlogFieldHandler 中完成的。
class CommonInfoEnumLoc {
 public:
  explicit CommonInfoEnumLoc(clang::DeclRefExpr* enum_ref): enum_ref_(enum_ref) {}
  explicit CommonInfoEnumLoc(clang::DeclRefExpr* enum_ref, clang::SourceLocation loc):
    enum_ref_(enum_ref),
    if_stmt_end_(loc) {}

  clang::DeclRefExpr* enum_ref() const { return enum_ref_; }
  const absl::optional<clang::SourceLocation>& if_stmt_end() const { return if_stmt_end_; }

  void set_bs_enum_str(const std::string& bs_enum_str) { bs_enum_str_ = bs_enum_str; }
  const std::string& bs_enum_str() const { return bs_enum_str_; }

 private:
  std::string bs_enum_str_;
  clang::DeclRefExpr* enum_ref_;
  absl::optional<clang::SourceLocation> if_stmt_end_;
};

/// 构造函数里的信息
class ConstructorInfo {
 public:
  ConstructorInfo() = default;
  explicit ConstructorInfo(const std::string& feature_name): feature_name_(feature_name) {}

  const std::string& feature_name() const { return feature_name_; }
  const std::string& feature_type() const { return feature_type_; }

  void set_body(clang::Stmt* body) { body_ = body; }
  clang::Stmt* body() { return body_; }

  void set_feature_type(const std::string& feature_type) { feature_type_ = feature_type; }
  void add_common_info_enum(clang::DeclRefExpr* expr) { common_info_enums_.emplace_back(expr); }
  void add_common_info_enum(clang::DeclRefExpr* expr, clang::SourceLocation loc) {
    common_info_enums_.emplace_back(expr, loc);
  }

  const std::vector<CommonInfoEnumLoc>& common_info_enums() const { return common_info_enums_; }
  std::vector<CommonInfoEnumLoc>& mutable_common_info_enums() { return common_info_enums_; }

  const clang::SourceLocation body_end() const { return body_end_; }
  void set_body_end(clang::SourceLocation loc) { body_end_ = loc; }

  void add_bs_field_enum(const std::string& bs_field_enum) { bs_field_enums_.insert(bs_field_enum); }
  void add_middle_node_leaf(const std::string& name) { middle_node_leafs_.insert(name); }

  const std::unordered_set<std::string>& bs_field_enums() const { return bs_field_enums_; }
  const std::unordered_set<std::string>& middle_node_leafs() const { return middle_node_leafs_; }

  void set_init_list(const std::string& init_list) { init_list_ = init_list; }
  const std::string& init_list() const { return init_list_; }

  void set_body_content(const std::string& body_content) { body_content_ = body_content; }
  const std::string& body_content() const { return body_content_; }

  void set_first_init_stmt(clang::CXXCtorInitializer * stmt) { first_init_stmt_ = stmt; }
  clang::CXXCtorInitializer * first_init_stmt() { return first_init_stmt_; }

  /// 去掉 bs_field_enums_ 中出现的 common_info_enums_
  void fix_common_info_enums();

  bool has_get_norm_query() const { return has_get_norm_query_; }
  void set_has_get_norm_query(bool v) { has_get_norm_query_ = v; }

  const VarDeclInfo& var_decl_info() const { return var_decl_info_; }
  VarDeclInfo& mutable_var_decl_info() { return var_decl_info_; }

  void add_param(const std::string& param) { params_.push_back(param); }
  const std::vector<std::string>& params() const { return (params_); }
  std::string joined_params() const;

  clang::SourceRange source_range() const { return source_range_; }
  void set_source_range(clang::SourceRange source_range) { source_range_ = source_range; }

  const std::unordered_map<std::string, AdlogFieldInfo>& adlog_field_infos() const {
    return (adlog_field_infos_);
  }
  void set_normal_adlog_field_info(const std::string& bs_enum_str,
                                   const std::string& adlog_field);
  void set_common_info_field_info(const std::string& bs_enum_str,
                                  const std::string& adlog_field,
                                  const std::string& common_info_enum_name,
                                  int common_info_value);

 private:
  clang::Stmt* body_;
  std::string feature_name_;
  std::string feature_type_;
  clang::SourceLocation body_end_;
  std::vector<CommonInfoEnumLoc> common_info_enums_;
  std::unordered_set<std::string> bs_field_enums_;
  std::unordered_set<std::string> middle_node_leafs_;
  std::string init_list_;
  std::string body_content_;
  clang::CXXCtorInitializer * first_init_stmt_;
  bool has_get_norm_query_ = false;
  VarDeclInfo var_decl_info_;
  std::vector<std::string> params_;
  clang::SourceRange source_range_;
  std::unordered_map<std::string, AdlogFieldInfo> adlog_field_infos_;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
