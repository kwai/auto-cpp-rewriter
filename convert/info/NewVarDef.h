#pragma once

#include <absl/types/optional.h>

#include <map>
#include <string>

#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/AST/AST.h"
#include "clang/AST/StmtCXX.h"
#include "clang/AST/ASTConsumer.h"

#include "../Type.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

enum class NewVarType {
  SCALAR,
  LIST,
  MAP
};

/// 来自 adlog 的字段的类型
enum class AdlogVarType {
  NONE,

  /// 普通字段，如 adlog.item.type, adlog.user_info.id
  NORMAL,

  /// 来自中间节点的叶子节点，如 photo_info->author_info()->id()
  MIDDLE_NODE_ROOT,

  MIDDLE_NODE_LEAF,

  /// common info, 但是 enum_value 是通过模板参数或者变量指定的。
  COMMON_INFO_FIXED,

  /// 来自中间节点的 common info, 如 photo_info->common_info_attr()
  COMMON_INFO_MIDDLE_NODE,

  COMMON_INFO_MULTI_INT_LIST,

  /// 来自 action_detail 的字段。
  ACTION_DETAIL_FIELD,

  /// 通过模板参数或者变量获取 action_detail
  ACTION_DETAIL_FIXED,

  /// GetPhotoText
  GET_PHOTO_TEXT
};

/// 新增加的变量定义，需要保存定义以及新的变量名, 新的变量名必须保证和已有的 var_decl 以及新增变量
/// 不重复, 注意必须也要查看 parent_ 范围内的变量。采取从后面按 _ 取字段的方式逐个实验。
/// 如 adlog_user_info_action_detail_follow_id, 从 follow_id 开始尝试，如果已被使用，则尝试
/// detail_follow_id, 以此类推，知道找到合法的变量名。
/// 中间节点以及 GetCommonInfo 类型的变量需要和普通字段进行区分，保存 root 信息以及 field 等信息。
/// 中间节点需要保存 root 以及 field, field 格式形如 author_info.id,
/// GetCommonInfo 需要保存 name_value_alias 信息，以便于和模板参数关联起来。
class NewVarDef {
 public:
  NewVarDef() = default;
  explicit NewVarDef(const std::string& bs_enum_str,
                     const std::string& name,
                     AdlogVarType adlog_var_type = AdlogVarType::NONE):
    bs_enum_str_(bs_enum_str),
    name_(name),
    adlog_var_type_(adlog_var_type) {}

  /// 类型确定, common info 使用的变量。
  explicit NewVarDef(const std::string& bs_enum_str,
                     const std::string& name,
                     const std::string& var_def,
                     NewVarType new_var_type,
                     AdlogVarType adlog_var_type = AdlogVarType::NONE):
    bs_enum_str_(bs_enum_str),
    name_(name),
    var_def_(var_def),
    new_var_type_(new_var_type),
    adlog_var_type_(adlog_var_type) {}

  const std::string& name() const { return name_; }
  void set_name(const std::string& name);

  AdlogVarType adlog_var_type() const { return adlog_var_type_; }

  const std::string& bs_enum_str() const { return (bs_enum_str_); }
  void set_bs_enum_str(const std::string& bs_enum_str) { bs_enum_str_ = bs_enum_str; }
  const std::string& var_def() const { return var_def_; }

  const absl::optional<NewVarType>& new_var_type() const { return (new_var_type_); }
  void set_new_var_type(NewVarType new_var_type) { new_var_type_.emplace(new_var_type); }

  const std::string& exists_name() const { return (exists_name_); }
  void set_exists_name(const std::string& exists_name) { exists_name_ = exists_name; }

  const std::string& exists_var_def() const { return (exists_var_def_); }

  void set_exists_var_def(const std::string& exists_name, const std::string& exists_var_def);
  void set_var_def(const std::string& var_def, NewVarType new_var_type);

  const absl::optional<ExprType>& expr_type() const { return expr_type_; }
  void set_expr_type(ExprType expr_type) { expr_type_.emplace(expr_type); }

  bool is_list() const;

  const std::string& adlog_field() const { return (adlog_field_); }
  void set_adlog_field(const std::string& adlog_field) { adlog_field_ = adlog_field; }

  const absl::optional<std::string>& list_inner_type() const { return (list_inner_type_); }
  absl::optional<std::string>& mutable_list_inner_type() { return (list_inner_type_); }
  void set_list_inner_type(const std::string& inner_type) { list_inner_type_.emplace(inner_type); }

  /// 中间节点需要设置
  void set_middle_node_root(const std::string& middle_node_root) {
    middle_node_root_.emplace(middle_node_root);
  }

  const absl::optional<std::string>& middle_node_root() const { return (middle_node_root_); }

  void set_middle_node_field(const std::string& middle_node_field) {
    middle_node_field_.emplace(middle_node_field);
  }
  const absl::optional<std::string>& middle_node_field() const { return (middle_node_field_); }

  /// GetCommonInfoFixed 需要设置
  void set_common_info_prefix_adlog(const std::string& common_info_prefix_adlog) {
    common_info_prefix_adlog_.emplace(common_info_prefix_adlog);
  }
  const absl::optional<std::string>& common_info_prefix_adlog() const {
    return (common_info_prefix_adlog_);
  }

  void set_common_info_name_value_alias(const std::string& common_info_name_value_alias) {
    common_info_name_value_alias_.emplace(common_info_name_value_alias);
  }
  const absl::optional<std::string>& common_info_name_value_alias() const {
    return (common_info_name_value_alias_);
  }

  void set_action_var_name(const std::string& action_var_name) {action_var_name_.emplace(action_var_name);}
  const absl::optional<std::string>& action_var_name() const { return (action_var_name_); }

 private:
  std::string bs_enum_str_;
  std::string name_;
  std::string var_def_;
  absl::optional<NewVarType> new_var_type_;
  std::string exists_name_;
  std::string exists_var_def_;
  absl::optional<ExprType> expr_type_;
  std::string adlog_field_;

  /// list inner_type
  absl::optional<std::string> list_inner_type_;

  AdlogVarType adlog_var_type_;
  absl::optional<std::string> middle_node_root_;
  absl::optional<std::string> middle_node_field_;
  absl::optional<std::string> common_info_prefix_adlog_;
  absl::optional<std::string> common_info_name_value_alias_;
  absl::optional<std::string> action_var_name_;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
