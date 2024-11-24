#pragma once

#include <absl/types/optional.h>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <set>
#include <vector>
#include <type_traits>

#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/AST/AST.h"
#include "clang/AST/StmtCXX.h"
#include "clang/AST/ASTConsumer.h"

#include "Traits.h"
#include "./info/Info.h"
#include "./info/IfInfo.h"
#include "./info/DeclInfo.h"
#include "./info/NewActionParam.h"
#include "./info/NewVarDef.h"
#include "./info/ConstructorInfo.h"
#include "./info/ActionDetailInfo.h"
#include "./info/ActionDetailFixedInfo.h"
#include "./info/FeatureInfo.h"
#include "./info/SeqListInfo.h"
#include "./info/AssignInfo.h"
#include "./info/BSFieldInfo.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

/// 保存解析 `ast` 节点时获取的各种信息，用于之后的改写。
///
/// 由于不同的改写规则需要不同的信息，而解析 `ast` 时并不知道此信息是用于哪个改写规则，因此
/// 采用一个类似全局的变量 `Env` 来将这些信息保存到不同的属性字段中，改写规则从 `Env` 获取需要的信息
/// 来进行处理。实际 `Env` 是保存在某个 `ast` 节点中，使得所有子节点都可以通过 `parent` 指针来访问。
///
/// 只有 `if` 和 `for`, `body` 需要 Env。
///
/// 需要遍历完所有的叶子节点才能拿到所有的信息，如 common info enum 是在 if 判断语句里, action detail no
/// 是在 find(no) 中。不同的信息格式不一样，每种信息对应了一个 struct。详细的 info 定义可参考 `info/Info.h`。
class Env {
 private:
  /// 父节点 `Env`，用于保存代码的递归结构信息。
  Env *parent_ = nullptr;

  /// 子节点。
  std::vector<Env *> children_;

  /// 当前 `Env` 直到 `root Env` 使用过的变量名，用于在插入新增变量时检查变量名是否重复。
  std::unordered_set<std::string> used_var_names_;

  /// 特征名。
  std::string feature_name_;

  /// 特征类型。
  std::string feature_type_;

  /// 是否是循环。
  bool is_loop_ = false;

  /// 是否是 `if` 语句。
  bool is_if_ = false;

  /// 是第几个 `if` 语句。
  int if_index_ = 0;

  /// 当前 `Env` 的 action。用于 `action_detail_info` 相关的逻辑。
  ///
  /// 示例:
  /// ```cpp
  /// int no = 1;
  /// auto action_detail = adlog.user_info().action_detail.find(no);
  /// ```
  int action_ = -1;

  /// 当前 `Env` 的 action 表达式。
  ///
  /// 示例:
  /// ```cpp
  /// int no = 1;
  /// ```
  clang::Expr *action_expr_ = nullptr;

  /// action 表达式对应的 `bs` 表达式。
  std::string bs_action_expr_;

  /// action 表达式对应的叶子字段。
  ///
  /// `action_detail_info` 中 `map` 保存的是 `message` 结构，取叶子节点需要通过嵌套结构进行获取。
  ///
  /// 示例:
  /// ```cpp
  /// int no = 1;
  /// const auto& ad_action = adlog.user_info().explore_long_term_ad_action();
  /// auto action_no_iter = ad_action.find(no);
  /// if (action_no_iter != ad_action.end()) {
  ///   const auto& action_no_list = action_no_iter->second.list();
  ///   for (int k = 0; k < action_no_list.size() && k < 100; ++k) {
  ///     uint64_t photo_id = action_no_list[k].photo_id();
  ///     ...
  ///   }
  /// }
  /// ```
  std::vector<std::string> action_leaf_fields_;

  /// 出现过的变量声明。`key` 是变量名，`value` 是解析的 `clang::Expr` 表达式。
  std::map<std::string, clang::Expr *> var_decls_;

  /// 循环变量名。
  std::vector<std::string> loop_var_names_;

  /// 循环表达式。
  std::vector<clang::Expr *> loop_exprs_;

  /// common attr 枚举 int 值。
  int common_attr_int_value_ = 0;

  /// 是否是 common attr 条件语句的子节点。
  bool is_child_common_attr_cond_ = false;

  /// 是否有 `if` 语句的子节点。
  bool has_if_in_children_ = false;

  /// 出现过的变量声明。`key` 是变量名，`value` 是解析的 `clang::DeclStmt` 表达式。
  std::map<std::string, clang::DeclStmt *> decl_stmts_;

  /// 删除过的变量名。
  std::set<std::string> deleted_vars_;

  /// `c++` `for range` 循环表达式。
  clang::CXXForRangeStmt *cxx_for_range_stmt_ = nullptr;

  /// `c++` `for` 循环表达式。
  clang::ForStmt *for_stmt_ = nullptr;

  /// `c++` `if` 语句。
  clang::IfStmt *if_stmt_ = nullptr;

  /// 第一个 `if` 语句。
  clang::IfStmt *first_if_stmt_ = nullptr;

  /// 第一个 `if` 语句是否是检查 item pos 条件的语句。
  ///
  /// 示例:
  /// ```cpp
  /// if (pos >= adlog.item_size())) {
  ///   ...
  /// }
  /// ```
  bool is_first_if_check_item_pos_cond_ = true;

  /// 第一个 `if` 语句是检查 item pos 条件语句，并且是包含处理逻辑。
  ///
  /// 示例:
  /// ```cpp
  /// if (adlog.item_size() >= pos) { ... }
  /// ```
  bool is_first_if_check_item_pos_include_cond_ = false;

  /// 字段是否来自 `reco` 字段。
  ///
  /// 示例:
  /// ```cpp
  /// auto id = adlog.user_info().reco_user_info().id();
  /// ```
  bool is_reco_user_info_ = false;

  /// 方法名。
  std::string method_name_;

  /// 新增变量定义。
  ///
  /// 用于中间节点逻辑插入新的 `field` 定义。
  std::map<std::string, absl::optional<NewVarDef>> new_defs_;

  /// adlog.user_info.xxx 这种格式, 创建 ActionDetailInfo 时候再转换。
  absl::optional<std::string> action_detail_prefix_adlog_;

  /// `if` 语句信息。
  absl::optional<IfInfo> if_info_;

  /// 循环信息。
  absl::optional<LoopInfo> loop_info_;

  /// 变量声明信息。
  absl::optional<DeclInfo> decl_info_;

  /// 二元操作信息。
  absl::optional<BinaryOpInfo> binary_op_info_;

  /// switch case 信息。
  absl::optional<SwitchCaseInfo> switch_case_info_;

  /// 赋值信息。
  absl::optional<AssignInfo> assign_info_;

  /// `action_detail` 字段相关信息。
  absl::optional<ActionDetailInfo> action_detail_info_;

  /// 通过模板参数传递的 `action number` 对应的 `action_detail` 字段固定信息。
  absl::optional<ActionDetailFixedInfo> action_detail_fixed_info_;

  /// 普通 `common info` 信息。如 `common info` 枚举等。
  absl::optional<CommonInfoNormal> common_info_normal_;

  /// 多个 `common info` 保存到 `vector` 中对应的信息。
  absl::optional<CommonInfoMultiMap> common_info_multi_map_;

  /// 多个 `common info` `int list` 对应的信息。
  absl::optional<CommonInfoMultiIntList> common_info_multi_int_list_;

  /// `common info` 对应的枚举通过模板参数传递对应的信息。
  absl::optional<CommonInfoFixed> common_info_fixed_;

  /// 多个 `common info` 对应的枚举通过模板参数传递。
  absl::optional<CommonInfoFixedList> common_info_fixed_list_;

  /// 用于枚举可能通过参数传递而出现在后面的情况，需要先创建 `common info` 部分信息，之后再更新枚举等。
  absl::optional<CommonInfoPrepare> common_info_prepare_;

  /// `PhotoInfo` 等中间节点信息。
  absl::optional<MiddleNodeInfo> middle_node_info_;

  /// `protobuf` `repeated` 字段对应的信息。
  absl::optional<SeqListInfo> seq_list_info_;

  /// `photo_list` 固定字段对应的信息。
  absl::optional<ProtoListInfo> proto_list_info_;

  /// `proto` 字段对应的 `bs` 字段信息。
  absl::optional<BSFieldInfo> bs_field_info_;

  /// 特征构造函数相关信息。只保留其指针，用于修改其内容。
  ConstructorInfo *constructor_info_ = nullptr;

  /// 当前 `Env` 所在的特征。
  FeatureInfo *feature_info_ = nullptr;

 public:
  Env() = default;

  /// 创建 `Env` 节点。传递其父节点作为参数。
  explicit Env(Env* parent): parent_(parent) {
    parent_->add_child(this);
  }

  /// 获取当前 `Env` 中所有变量声明。
  const std::map<std::string, clang::Expr*>& var_decls() const { return var_decls_; }

  /// 获取当前 `Env` 的父节点。
  Env* parent() { return parent_; }

  /// 获取当前 `Env` 的 `const` 父节点。
  const Env* parent() const { return parent_; }

  /// 当前 `Env` 是否是根节点。
  bool is_root() { return parent_ == nullptr; }

  /// 获取当前 `Env` 的 `const` 根节点。
  const Env* get_root() const;

  /// 获取当前 `Env` 的可变根节点。
  Env* get_mutable_root();

  /// 获取 `common info` 预备信息所在的 `const Env`。
  const Env* get_common_info_prepare_env() const;

  /// 获取 `common info` 预备信息所在的 `Env`。
  Env* mutable_common_info_prepare_env();

  /// 获取 `common info` 父节点所在的 `const Env`。
  const Env* get_common_info_parent_env() const;

  /// 获取 `common info` 父节点所在的可变 `Env`。
  Env* mutable_common_info_parent_env();

  /// 当前 `Env` 父节点是否是循环。
  bool is_parent_loop() const;

  /// 当前 `Env` 父节点是否是 `if` 语句。
  bool is_parent_if() const;

  /// 添加使用过的变量名。
  /// Env 可能是局部变量, 因此用过的变量名必须保存在 root 里。
  void add_used_var_name(const std::string& name);

  /// 变量名是否被使用过，需要判断直到根节点的所有节点。
  bool is_var_name_used(const std::string& name) const;

  /// 添加模板变量名。
  void add_template_var_names(const std::vector<std::string>& var_names);

  /// 设置特征名。
  void set_feature_name(const std::string& feature_name);

  /// 获取特征名。
  const std::string& feature_name() const;

  /// 设置特征类型。
  void set_feature_type(const std::string& feature_type);

  /// 获取特征类型。
  const std::string& feature_type() const;

  /// 当前 `Env` 是否是循环。
  bool is_loop() const { return is_loop_; }

  /// 设置当前 `Env` 是否是循环。
  void set_is_loop(bool is_loop) { is_loop_ = is_loop; }

  /// 获取循环 `Env` 的 `const` 父节点。
  ///
  /// 如果不是循环节点，则返回 `nullptr`。
  const Env* get_loop_parent() const;

  /// 获取循环 `Env` 的可变父节点。
  Env* mutable_loop_parent();

  // 获取最外层的 `const loop Env`, 可能有多层 loop 嵌套。
  const Env* get_outer_loop() const;

  // 最外层的 mutable loop, 可能有多层 loop 嵌套。
  Env* mutable_outer_loop();

  /// 获取最外层的 `loop` 的父节点。
  const Env* get_outer_loop_parent() const;

  /// 获取最外层的 `loop` 的可变父节点。
  Env* mutable_outer_loop_parent();

  /// 获取外层 `if` `Env`。
  const Env *get_outer_if() const;

  /// 获取外层 `if` 的可变 `Env`。
  Env *mutable_outer_if();

  /// 获取外层 `if` 对应的父节点。
  const Env *get_outer_if_parent() const;

  /// 获取外层 `if` 对应的 `mutable` 父节点。
  Env *mutable_outer_if_parent();

  /// 获取循环 `Env`。
  const Env* get_loop_env() const;

  /// 获取循环 `Env` 的可变版本。
  Env* mutable_loop_env();

  /// 添加子节点。
  void add_child(Env* child);

  /// 将解析得到的 `Expr` 添加到 `map` 中。
  void add(const std::string& key, clang::Expr* expr);

  /// 根据变量名查找代码中的变量。
  clang::Expr* find(const std::string& key) const;

  /// 从 `map` 中删除变量。
  void erase(const std::string& key);

  /// 添加循环变量。
  void add_loop_var(const std::string& key);

  /// 获取循环变量。
  void pop_loop_var();

  /// 获取最后一个循环变量。用于有多个循环变量的情况。
  const std::string& get_last_loop_var() const;

  /// 变量是否是循环变量。
  bool is_loop_var(const std::string& key) const;

  /// 当前 `Env` 是否是在循环中。
  bool is_in_loop() const;

  /// 获取循环变量名。
  const std::vector<std::string>& loop_var_names() const { return loop_var_names_; }

  /// 当前 `Env` 是否是 `if` 语句。
  bool is_if() const { return is_if_; }

  /// 设置当前 `Env` 是否是 `if` 语句。
  void set_is_if(bool is_if);

  /// 当前 `Env` 是否是第一个 `if` 语句。
  bool is_first_if() const { return is_if_ && parent_ != nullptr && parent_->if_index() == 1; }

  /// 获取 `if` 语句索引。用于有多个 `if` 语句的情况。
  int if_index() const { return if_index_; }

  /// 增加 `if` 语句索引。
  void increase_if_index() { if_index_++; }

  /// 当前 `Env` 是否是在 `if` 语句中。
  bool is_in_if() const;

  /// 当前 `Env` 是否是在 `if` 语句条件中。
  bool is_in_if_cond() const;

  /// 当前 `Env` 是否是在 `if` 语句体中。
  bool is_in_if_body() const;

  /// 当前 `Env` 是否有 `if` 语句的子节点。
  bool has_if_in_children() const;

  /// 设置 `has_if_in_children`。
  void set_has_if_in_children(bool v);

  /// 当前 `Env` 是否是在检查中间节点的条件。
  bool is_check_middle_node_root_cond() const;

  /// 添加循环 `Expr`。
  void add_loop_expr(clang::Expr* expr);

  /// 获取循环 `Expr` 数量。
  size_t get_loop_expr_size();

  /// 获取循环 `Expr`。
  clang::Expr* get_loop_expr(size_t index);

  /// 获取第一个循环 `Expr`。
  clang::Expr* get_first_loop_expr();

  /// 获取父循环 `Expr`。
  clang::Expr* find_parent_loop();

  /// 当前 `Env` 是否是组合特征。
  bool is_combine_feature() const;

  /// 当前 `Env` 是否是用户特征。
  bool is_user_feature() const;

  /// 当前 `Env` 是否是 item 特征。
  bool is_item_feature() const;

  /// 当前 `Env` 是否是 `sparse` 特征。
  bool is_sparse_feature() const;

  /// 当前 `Env` 是否是 `dense` 特征。
  bool is_dense_feature() const;

  /// 是否是 common info list 或者 map 的遍历 loop
  bool is_common_info_loop() const;

  /// 父节点是否是 common info list 或者 map 的遍历 loop
  bool is_parent_common_info_loop() const;

  /// 当前 `Env` 是否是在 common info list 或者 map 的遍历 loop 体中。
  bool is_in_common_info_loop_body() const;

  /// 当前 `Env` 是否是在 common info 的 `if` 语句体中。
  bool is_in_common_info_if_body() const;

  /// 获取 common info 的 枚举 int 值。
  absl::optional<int> get_common_attr_int_value() const;

  /// 获取 common info 的 枚举名字符串。
  absl::optional<std::string> get_common_attr_int_name() const;

  /// 是否是在检查 item 位置条件。
  bool is_check_item_pos_cond() const;

  /// 是否是在检查 action detail 条件。
  bool is_action_detail_cond() const;

  /// 设置是否是 common attr 判断条件。
  void set_is_child_common_attr_cond(bool v);

  /// 当前 `Env` 是否是 common attr 判断条件。
  bool is_child_common_attr_cond() const;

  /// 设置 `CXXForRangeStmt`。
  void set_cxx_for_range_stmt(clang::CXXForRangeStmt* cxx_for_range_stmt);

  /// 获取 `CXXForRangeStmt`。
  clang::CXXForRangeStmt* cxx_for_range_stmt() const;

  /// 设置 `ForStmt`。
  void set_for_stmt(clang::ForStmt* for_stmt);

  /// 获取 `ForStmt`。
  clang::ForStmt* for_stmt() const;

  /// 设置 `IfStmt`。
  void set_if_stmt(clang::IfStmt* if_stmt);

  /// 获取 `IfStmt`。
  clang::IfStmt* if_stmt() const;

  /// 是否所有 `if` 语句都被删除。
  bool is_all_if_stmt_deleted() const;

  /// 添加 `DeclStmt`。
  void add_decl_stmt(const std::string& name, clang::DeclStmt* decl_stmt);

  /// 获取原始的 `DeclStmt`。
  clang::DeclStmt* get_decl_stmt(const std::string& name) const;

  /// 获取当前 `Env` 中的 `DeclStmt`。
  clang::DeclStmt* get_decl_stmt_in_cur_env(const std::string& name) const;

  /// 查找 `DeclStmt` 的 `Env`。
  Env* find_decl_env(const std::string& name);

  /// 变量是否在 `DeclStmt`。
  bool is_var_in_decl_stmt(const std::string& name) const;

  /// 添加删除的变量。
  void add_deleted_var(const std::string& name);

  /// 通过 `Expr` 添加删除的变量。
  bool add_deleted_var_by_expr(clang::Expr* expr);

  /// 通过 `Expr` 字符串添加删除的变量。
  bool add_deleted_var_by_expr_str(const std::string& expr_str);

  /// 获取删除的变量。
  const std::set<std::string>& deleted_vars() const { return deleted_vars_; }

  /// 删除变量。用于删除改写后不再需要的 `proto` 变量相关逻辑。
  void pop_deleted_var(const std::string& name);

  /// 清楚需要删除的列表。
  void clear_deleted_var();

  /// 一些语句中的变量定义会在之后的语句中继续使用，因此不能定义在当前 Env 中，必须往上寻找合适的 Env。
  /// 比如 if 语句, common info body, loop body 等。
  /// if 只有 cond 中的语句可能会有这种现象，因此目前对于 if 只考虑 if conf 中的语句。
  Env* mutable_new_def_target_env(bool is_from_reco);

  /// 添加新的变量定义。
  ///
  /// 如果是已经在代码里定义的变量, 则直接用其变量名，不再重新新增定义, 并且将其初始化直接替换为 bs 表达式。
  void add_new_def(const std::string& bs_enum_str,
                   const std::string& var_def,
                   NewVarType new_var_type);

  /// 添加新的定义辅助函数。
  void add_new_def_helper(const std::string &bs_enum_str,
                          const std::string &var_def,
                          NewVarType new_var_type);

  /// 添加新的变量 meta 信息。如变量类型，字段路径等。
  void add_new_def_meta(const std::string& bs_enum_str,
                        const std::string& var_def,
                        NewVarType new_var_type);

  /// 添加 decl_stmt 的 adlog 变量
  void add_new_def(const std::string& bs_enum_str,
                   const std::string& var_name,
                   const std::string& var_def,
                   NewVarType new_var_type);

  /// 添加新的定义辅助函数。
  void add_new_def_helper(const std::string &bs_enum_str,
                          const std::string &var_name,
                          const std::string &var_def,
                          NewVarType new_var_type);

  /// 添加新的变量 meta 信息。如变量类型，字段路径等。
  void add_new_def_meta(const std::string& bs_enum_str,
                        const std::string& var_name,
                        const std::string& var_def,
                        NewVarType new_var_type);

  /// 添加已经存在的变量定义。用于已经声明的变量。
  void add_new_exists_def(const std::string& bs_enum_str,
                          const std::string& exists_var_def);

  /// 添加已经存在的变量定义辅助函数。
  void add_new_exists_def_helper(const std::string& bs_enum_str,
                                 const std::string& exists_var_def);

  /// 添加已经存在的变量 meta 信息。如变量类型，字段路径等。
  void add_new_exists_def_meta(const std::string& bs_enum_str,
                                const std::string& exists_var_def);

  /// 用于修复 bs field, 直接使用 bs_var_name 为变量名。
  void add_new_def_overwrite(const std::string &bs_var_name,
                             const std::string &var_def,
                             NewVarType new_var_type);

  /// 添加 `bs_enum_str` 作为 `meta` 信息。
  void add_attr_meta(const std::string& bs_enum_str);

  /// 添加字段详细信息，包括:
  /// 1. adlog_field, 如: adlog.user_info.id
  /// 2. bs_field_enum, 如: adlog_user_info_id
  void set_normal_adlog_field_info(const std::string& bs_enum_str,
                                   const std::string& adlog_field);

  /// 添加字段详细信息，包括:
  /// 1. adlog_field, 如: adlog.user_info.id
  /// 2. bs_field_enum, 如: adlog_user_info_id
  /// 3. enum_value, 如果是 CommonInfo
  /// 4. enum_str, 如果是 CommonInfo
  void set_common_info_field_info(const std::string& bs_enum_str,
                                  const std::string& adlog_field,
                                  const std::string& common_info_enum_name,
                                  int common_info_value);

  /// 根据 `bs_enum_str` 查找新的变量定义。
  const absl::optional<NewVarDef>& find_new_def(const std::string& bs_enum_str) const;

  /// 根据 `bs_enum_str` 查找新的 `mutable` 变量定义。用于更新信息。
  absl::optional<NewVarDef>& find_mutable_new_def(const std::string& bs_enum_str);

  /// 获取所有新的变量定义。用于最后添加代码。
  const std::map<std::string, absl::optional<NewVarDef>>& new_defs() const { return new_defs_; }

  /// 根据字符串拼接规则查找有效的新的变量名。
  std::string find_valid_new_name(const std::string& bs_enum_str) const;

  /// 变量名是否有效。
  bool is_new_name_valid(const std::string& name) const;

  /// 新变量名是否存在。
  bool is_new_var_exists(const std::string& bs_enum_str) const;

  /// 变量名是否不存在。
  bool is_new_var_not_exists(const std::string& bs_enum_str) const;

  /// 根据 `bs_enum_str` 获取新的变量名。
  const std::string& find_new_var_name(const std::string& bs_enum_str) const;

  /// 根据 `bs_enum_str` 获取已经存在的变量名。
  const std::string& find_new_exists_var_name(const std::string& bs_enum_str) const;

  /// 获取所有新的变量名。
  std::vector<std::string> get_all_new_def_var_names() const;

  /// 获取所有新的变量定义。
  std::string get_all_new_defs() const;

  /// 设置第一个 `if` 语句。
  void set_first_if_stmt(clang::IfStmt* if_stmt) { first_if_stmt_ = if_stmt; }

  /// 获取第一个 `if` 语句。
  clang::IfStmt* first_if_stmt() const { return first_if_stmt_; }

  /// 设置是否是第一个 `if` 语句的 item 位置条件检查。
  void set_is_first_if_check_item_pos_cond(bool v) { is_first_if_check_item_pos_cond_ = v; }

  /// 是否是第一个 `if` 语句的 item 位置条件检查。
  bool is_first_if_check_item_pos_cond() const { return is_first_if_check_item_pos_cond_; }

  /// 设置是否是第一个 `if` 语句的 item 位置条件检查包含其他逻辑。
  void set_is_first_if_check_item_pos_include_cond(bool v) { is_first_if_check_item_pos_include_cond_ = v; }

  /// 第一个 `if` 语句的 item 位置条件检查是否包含其他逻辑。
  bool is_first_if_check_item_pos_include_cond() const { return is_first_if_check_item_pos_include_cond_; }

  /// 设置 action number。
  void set_action(int action) { action_ = action; }

  /// 获取 action number。
  int get_action();

  /// 设置 action 表达式。action 可能是变量。
  void set_action_expr(clang::Expr* expr, std::string bs_action_expr);

  /// 获取 action 表达式。
  std::string get_bs_action_expr();

  /// 根据 `bs_enum_str` 查找一个 action detail 叶子变量名。
  std::string find_one_action_detail_leaf_name(const std::string& bs_enum_str) const;

  /// 添加 `IfStmt`。
  void add_if_stmt(clang::IfStmt* if_stmt);

  /// 添加 `ForStmt`。
  void add_loop_stmt(clang::ForStmt* for_stmt);

  /// 添加 `CXXForRangeStmt`。
  void add_loop_stmt(clang::CXXForRangeStmt* cxx_for_range_stmt);

  /// 添加 action detail 字段前缀, 如 `adlog.user_info.longterm_action_detail`。
  void add_action_detail_prefix_adlog(const std::string& prefix_adlog);

  /// 添加中间节点名。如 `PhotoInfo`。
  void add_middle_node_name(const std::string& name);

  /// 设置特征信息对应的指针。
  void set_feature_info(FeatureInfo* feature_info);

  /// 设置构造函数信息的指针，用于修改其内容。
  void set_constructor_info(ConstructorInfo* constructor_info);

  /// 在 `common info` 第一次出现的地方创建 `CommonInfoNormal` 信息。
  ///
  /// common info 第一次出现的地方一定是取 repeated common info 字段，因此可以知道 prefix, 并且 prefix 一定是
  /// 最早出现的, name_value 在之后出现。
  absl::optional<CommonInfoNormal>& touch_common_info_normal();

  /// 在通过模板参数传递的 `common info` 第一次出现的地方创建 `CommonInfoFixedList` 信息。
  absl::optional<CommonInfoFixedList>& touch_common_info_fixed_list();

  /// 在多个 `common info` 第一次出现的地方创建 `CommonInfoMultiMap` 信息。
  absl::optional<CommonInfoMultiMap>& touch_common_info_multi_map(const std::string& map_name,
                                                                  const std::string& attr_name);

  /// 在多个 `common info` 第一次出现的地方创建 `CommonInfoMultiIntList` 信息。
  absl::optional<CommonInfoMultiIntList> & touch_common_info_multi_int_list();

  /// 在 `action detail` 第一次出现的地方创建 `ActionDetailInfo` 信息。
  absl::optional<ActionDetailInfo>& touch_action_detail_info(int action);

  /// 更新 `action detail` 信息。
  absl::optional<ActionDetailInfo>& update_action_detail_info(int action);

  /// 在 `action detail` 第一次出现的地方创建 `ActionDetailFixedInfo` 信息。
  absl::optional<ActionDetailFixedInfo>& touch_action_detail_fixed_info(const std::string& action);

  /// 在 `seq list` 第一次出现的地方创建 `SeqListInfo` 信息。
  absl::optional<SeqListInfo>& touch_seq_list_info(const std::string& root_name);

  /// 在 `photo list` 第一次出现的地方创建 `ProtoListInfo` 信息。
  absl::optional<ProtoListInfo>& touch_proto_list_info(const std::string& prefix_adlog);

  /// 在 `bs field` 第一次出现的地方创建 `BSFieldInfo` 信息。
  absl::optional<BSFieldInfo>& touch_bs_field_info(const std::string& bs_var_name);

  /// 设置 `common_info` 前缀。
  ///
  /// 示例: adlog.user_info.common_info_attr。
  void set_common_info_prefix_adlog(const std::string& prefix_adlog);

  /// 获取 `common_info` 前缀。
  const absl::optional<std::string>& common_info_prefix() const;

  /// 获取 `common_info` 前缀。
  const absl::optional<std::string>& get_common_info_prefix() const;

  /// 更新模板 `common_info` 的值。
  void update_template_common_info_values();

  /// 添加 `common_info` 详细定义。
  void add_common_info_detail_def(const CommonInfoLeaf& common_info_detail);

  /// 添加构造函数声明。
  void add_ctor_decls(const VarDeclInfo& var_decl_info);

  // 获取模板参数名
  absl::optional<std::string> get_template_int_name(clang::Expr* init_expr) const;

  /// 是否是模板参数引用。
  bool is_template_int_ref(clang::Expr* expr) const;

  /// 是否是 reco user info。
  bool is_reco_user_info() const;

  /// 设置是否是 reco user info。
  void set_is_reco_user_info(bool v);

  /// 获取当前 `Env` 所在的方法名。
  const std::string& get_method_name() const;

  /// 方法名。
  const std::string& method_name() const { return method_name_; }

  /// 设置方法名。
  void set_method_name(const std::string& method_name) { method_name_ = method_name; }

  /// 是否是 feature 其他方法。
  bool is_feature_other_method(const std::string& method_name) const;

  /// 添加 action 参数的新定义。
  void add_action_param_new_def(const std::string& prefix,
                                 const NewActionParam& new_action_param);

  /// 查找 action 参数的新定义变量名。
  std::vector<std::string> find_new_action_param_var_name(const std::string& prefix,
                                                          const NewActionParam& new_action_param) const;

  /// 清除 `common_info_fixed_list`。
  void clear_common_info_fixed_list();

  /// 是否在循环初始化语句中。
  bool is_in_loop_init() const;

  /// 是否在循环体中。
  bool is_in_loop_body() const;

  /// 是否在循环初始化语句中。
  bool is_in_for_range_init() const;

  /// `Decl` 表达式是否包含自身的引用。
  ///
  /// 计算表达式可能会包含引用到自身的变量。
  bool is_decl_ref_contains_self(clang::DeclRefExpr* decl_ref_expr,
                                 clang::Expr* value_expr) const;


  /// 当前 `Env` 的 `const` `if` 信息。
  const absl::optional<IfInfo>& cur_if_info() const { return if_info_; }

  /// 当前 `Env` 的 `const` `loop` 信息。
  const absl::optional<LoopInfo>& cur_loop_info() const { return loop_info_; }

  /// 当前 `Env` 的 `const` `switch case` 信息。
  const absl::optional<SwitchCaseInfo>& cur_switch_case_info() const { return switch_case_info_; }

  /// 当前 `Env` 的 `const` `Decl` 信息。
  const absl::optional<DeclInfo>& cur_decl_info() const { return decl_info_; }

  /// 当前 `Env` 的 `const` `BinaryOperator` 信息。
  const absl::optional<BinaryOpInfo>& cur_binary_op_info() const { return binary_op_info_; }

  /// 当前 `Env` 的 `const` `AssignInfo` 信息。
  const absl::optional<AssignInfo>& cur_assign_info() const { return assign_info_; }

  /// 当前 `Env` 的 `const` `ActionDetailInfo` 信息。
  const absl::optional<ActionDetailInfo>& cur_action_detail_info() const { return action_detail_info_; }

  /// 当前 `Env` 的 `const` `ActionDetailFixedInfo` 信息。
  const absl::optional<ActionDetailFixedInfo>& cur_action_detail_fixed_info() const {
    return action_detail_fixed_info_;
  }

  /// 当前 `Env` 的 `const` `CommonInfoNormal` 信息。
  const absl::optional<CommonInfoNormal>& cur_common_info_normal() const { return common_info_normal_; }

  /// 当前 `Env` 的 `const` `CommonInfoMultiMap` 信息。
  const absl::optional<CommonInfoMultiMap>& cur_common_info_multi_map() const {
    return common_info_multi_map_;
  }

  /// 当前 `Env` 的 `const` `CommonInfoMultiIntList` 信息。
  const absl::optional<CommonInfoMultiIntList>& cur_common_info_multi_int_list() const {
    return common_info_multi_int_list_;
  }

  /// 当前 `Env` 的 `const` `CommonInfoFixed` 信息。
  const absl::optional<CommonInfoFixed>& cur_common_info_fixed() const { return common_info_fixed_; }

  /// 当前 `Env` 的 `const` `CommonInfoFixedList` 信息。
  const absl::optional<CommonInfoFixedList>& cur_common_info_fixed_list() const {
    return common_info_fixed_list_;
  }

  /// 当前 `Env` 的 `const` `CommonInfoPrepare` 信息。
  const absl::optional<CommonInfoPrepare>& cur_common_info_prepare() const { return common_info_prepare_; }

  /// 当前 `Env` 的 `const` `MiddleNodeInfo` 信息。
  const absl::optional<MiddleNodeInfo>& cur_middle_node_info() const { return middle_node_info_; }

  /// 当前 `Env` 的 `const` `SeqListInfo` 信息。
  const absl::optional<SeqListInfo>& cur_seq_list_info() const { return seq_list_info_; }

  /// 当前 `Env` 的 `const` `ProtoListInfo` 信息。
  const absl::optional<ProtoListInfo>& cur_proto_list_info() const { return proto_list_info_; }

  /// 当前 `Env` 的 `const` `BSFieldInfo` 信息。
  const absl::optional<BSFieldInfo>& cur_bs_field_info() const { return bs_field_info_; }


  /// 当前 `Env` 的 `mutable` `if` 信息。
  absl::optional<IfInfo>& cur_mutable_if_info() { return if_info_; }

  /// 当前 `Env` 的 `mutable` `loop` 信息。
  absl::optional<LoopInfo>& cur_mutable_loop_info() { return loop_info_; }

  /// 当前 `Env` 的 `mutable` `switch case` 信息。
  absl::optional<SwitchCaseInfo>& cur_mutable_switch_case_info() { return switch_case_info_; }

  /// 当前 `Env` 的 `mutable` `Decl` 信息。
  absl::optional<DeclInfo>& cur_mutable_decl_info() { return decl_info_; }

  /// 当前 `Env` 的 `mutable` `BinaryOperator` 信息。
  absl::optional<BinaryOpInfo>& cur_mutable_binary_op_info() { return binary_op_info_; }

  /// 当前 `Env` 的 `mutable` `AssignInfo` 信息。
  absl::optional<AssignInfo>& cur_mutable_assign_info() { return assign_info_; }

  /// 当前 `Env` 的 `mutable` `ActionDetailInfo` 信息。
  absl::optional<ActionDetailInfo>& cur_mutable_action_detail_info() { return action_detail_info_; }

  /// 当前 `Env` 的 `mutable` `ActionDetailFixedInfo` 信息。
  absl::optional<ActionDetailFixedInfo>& cur_mutable_action_detail_fixed_info() {
    return action_detail_fixed_info_;
  }

  /// 当前 `Env` 的 `mutable` `CommonInfoNormal` 信息。
  absl::optional<CommonInfoNormal>& cur_mutable_common_info_normal() { return common_info_normal_; }

  /// 当前 `Env` 的 `mutable` `CommonInfoMultiMap` 信息。
  absl::optional<CommonInfoMultiMap>& cur_mutable_common_info_multi_map() { return common_info_multi_map_; }

  /// 当前 `Env` 的 `mutable` `CommonInfoMultiIntList` 信息。
  absl::optional<CommonInfoMultiIntList>& cur_mutable_common_info_multi_int_list() {
    return common_info_multi_int_list_;
  }

  /// 当前 `Env` 的 `CommonInfoFixed` 信息。
  absl::optional<CommonInfoFixed>& cur_mutable_common_info_fixed() { return common_info_fixed_; }

  /// 当前 `Env` 的 `mutable` `CommonInfoFixedList` 信息。
  absl::optional<CommonInfoFixedList>& cur_mutable_common_info_fixed_list() {
    return common_info_fixed_list_;
  }

  /// 当前 `Env` 的 `mutable` `CommonInfoPrepare` 信息。
  absl::optional<CommonInfoPrepare>& cur_mutable_common_info_prepare() { return common_info_prepare_; }

  /// 当前 `Env` 的 `mutable` `MiddleNodeInfo` 信息。
  absl::optional<MiddleNodeInfo>& cur_mutable_middle_node_info() { return middle_node_info_; }

  /// 当前 `Env` 的 `mutable` `SeqListInfo` 信息。
  absl::optional<SeqListInfo>& cur_mutable_seq_list_info() { return seq_list_info_; }

  /// 当前 `Env` 的 `mutable` `ProtoListInfo` 信息。
  absl::optional<ProtoListInfo>& cur_mutable_proto_list_info() { return proto_list_info_; }

  /// 当前 `Env` 的 `mutable` `BSFieldInfo` 信息。
  absl::optional<BSFieldInfo>& cur_mutable_bs_field_info() { return bs_field_info_; }

  /// 当前 `Env` 的 `mutable` `if` 信息。
  absl::optional<IfInfo>& cur_info(const IfInfo& v) { return if_info_; }

  /// 当前 `Env` 的 `mutable` `loop` 信息。
  absl::optional<LoopInfo>& cur_info(const LoopInfo& v) { return loop_info_; }

  /// 当前 `Env` 的 `mutable` `switch case` 信息。
  absl::optional<SwitchCaseInfo>& cur_info(const SwitchCaseInfo& v) { return switch_case_info_; }

  /// 当前 `Env` 的 `mutable` `Decl` 信息。
  absl::optional<DeclInfo>& cur_info(const DeclInfo& v) { return decl_info_; }

  /// 当前 `Env` 的 `mutable` `BinaryOperator` 信息。
  absl::optional<BinaryOpInfo>& cur_info(const BinaryOpInfo& v) { return binary_op_info_; }

  /// 当前 `Env` 的 `mutable` `AssignInfo` 信息。
  absl::optional<AssignInfo>& cur_info(const AssignInfo& v) { return assign_info_; }

  /// 当前 `Env` 的 `ActionDetailInfo` 信息。
  absl::optional<ActionDetailInfo>& cur_info(const ActionDetailInfo& v) { return action_detail_info_; }

  /// 当前 `Env` 的 `mutable` `ActionDetailFixedInfo` 信息。
  absl::optional<ActionDetailFixedInfo>& cur_info(const ActionDetailFixedInfo& v) {
    return action_detail_fixed_info_;
  }

  /// 当前 `Env` 的 `mutable` `CommonInfoNormal` 信息。
  absl::optional<CommonInfoNormal>& cur_info(const CommonInfoNormal& v) { return common_info_normal_; }

  /// 当前 `Env` 的 `mutable` `CommonInfoMultiMap` 信息。
  absl::optional<CommonInfoMultiMap>& cur_info(const CommonInfoMultiMap& v) {
    return common_info_multi_map_;
  }

  /// 当前 `Env` 的 `CommonInfoMultiIntList` 信息。
  absl::optional<CommonInfoMultiIntList>& cur_info(const CommonInfoMultiIntList& v) {
    return common_info_multi_int_list_;
  }

  /// 当前 `Env` 的 `mutable` `CommonInfoFixed` 信息。
  absl::optional<CommonInfoFixed>& cur_info(const CommonInfoFixed& v) { return common_info_fixed_; }

  /// 当前 `Env` 的 `mutable` `CommonInfoFixedList` 信息。
  absl::optional<CommonInfoFixedList>& cur_info(const CommonInfoFixedList& v) {
    return common_info_fixed_list_;
  }

  /// 当前 `Env` 的 `mutable` `CommonInfoPrepare` 信息。
  absl::optional<CommonInfoPrepare>& cur_info(const CommonInfoPrepare& v) { return common_info_prepare_; }

  /// 当前 `Env` 的 `mutable` `MiddleNodeInfo` 信息。
  absl::optional<MiddleNodeInfo>& cur_info(const MiddleNodeInfo& v) { return middle_node_info_; }

  /// 当前 `Env` 的 `mutable` `SeqListInfo` 信息。
  absl::optional<SeqListInfo>& cur_info(const SeqListInfo& v) { return seq_list_info_; }

  /// 当前 `Env` 的 `mutable` `ProtoListInfo` 信息。
  absl::optional<ProtoListInfo>& cur_info(const ProtoListInfo& v) { return proto_list_info_; }

  /// 当前 `Env` 的 `mutable` `BSFieldInfo` 信息。
  absl::optional<BSFieldInfo>& cur_info(const BSFieldInfo& v) { return bs_field_info_; }

  const absl::optional<IfInfo>& cur_info(const IfInfo& v) const { return if_info_; }

  const absl::optional<LoopInfo>& cur_info(const LoopInfo& v) const { return loop_info_; }

  const absl::optional<SwitchCaseInfo>& cur_info(const SwitchCaseInfo& v) const {
    return switch_case_info_;
  }

  const absl::optional<DeclInfo>& cur_info(const DeclInfo& v) const { return decl_info_; }
  const absl::optional<BinaryOpInfo>& cur_info(const BinaryOpInfo& v) const { return binary_op_info_; }
  const absl::optional<AssignInfo>& cur_info(const AssignInfo& v) const { return assign_info_; }
  const absl::optional<ActionDetailInfo>& cur_info(const ActionDetailInfo& v) const {
    return action_detail_info_;
  }
  const absl::optional<ActionDetailFixedInfo>& cur_info(const ActionDetailFixedInfo& v) const {
    return action_detail_fixed_info_;
  }
  const absl::optional<CommonInfoNormal>& cur_info(const CommonInfoNormal& v) const {
    return common_info_normal_;
  }
  const absl::optional<CommonInfoMultiMap>& cur_info(const CommonInfoMultiMap& v) const {
    return common_info_multi_map_;
  }
  const absl::optional<CommonInfoMultiIntList>& cur_info(const CommonInfoMultiIntList& v) const {
    return common_info_multi_int_list_;
  }
  const absl::optional<CommonInfoFixed>& cur_info(const CommonInfoFixed& v) const {
    return common_info_fixed_;
  }
  const absl::optional<CommonInfoFixedList>& cur_info(const CommonInfoFixedList& v) const {
    return common_info_fixed_list_;
  }
  const absl::optional<CommonInfoPrepare>& cur_info(const CommonInfoPrepare& v) const {
    return common_info_prepare_;
  }
  const absl::optional<MiddleNodeInfo>& cur_info(const MiddleNodeInfo& v) const {
    return middle_node_info_;
  }
  const absl::optional<SeqListInfo>& cur_info(const SeqListInfo& v) const {
    return seq_list_info_;
  }
  const absl::optional<ProtoListInfo>& cur_info(const ProtoListInfo& v) const {
    return proto_list_info_;
  }

  const absl::optional<BSFieldInfo> &cur_info(const BSFieldInfo &v) const {return bs_field_info_;}

  /// 获取 `info` 的通用模板函数。从当前 `Env` 开始查找，如果当前 `Env` 没有，则查找父 `Env`, 一直到根节点。
  template<typename T>
  const absl::optional<T>& get_info() const {
    const absl::optional<T>& info = cur_info(InfoTraits<T>::v);
    if (info.has_value()) {
      return info;
    }

    if (parent_ != nullptr) {
      return parent_->get_info<T>();
    }

    static const absl::optional<T> empty;
    return empty;
  }

  template<typename T>
  absl::optional<T>& get_info() {
    const absl::optional<T>& info = const_cast<const Env*>(this)->get_info<T>();
    return const_cast<absl::optional<T>&>(info);
  }

  const absl::optional<IfInfo>& get_if_info() const { return get_info<IfInfo>(); }
  const absl::optional<LoopInfo>& get_loop_info() const { return get_info<LoopInfo>(); }
  const absl::optional<SwitchCaseInfo>& get_switch_case_info() const { return get_info<SwitchCaseInfo>(); }
  const absl::optional<DeclInfo>& get_decl_info() const { return get_info<DeclInfo>(); }
  const absl::optional<BinaryOpInfo>& get_binary_op_info() const { return get_info<BinaryOpInfo>(); }
  const absl::optional<AssignInfo>& get_assign_info() const { return get_info<AssignInfo>(); }
  const absl::optional<ActionDetailInfo>& get_action_detail_info() const {
    return get_info<ActionDetailInfo>();
  }
  const absl::optional<ActionDetailFixedInfo>& get_action_detail_fixed_info() const {
    return get_info<ActionDetailFixedInfo>();
  }
  const absl::optional<CommonInfoNormal>& get_common_info_normal() const {
    return get_info<CommonInfoNormal>();
  }
  const absl::optional<CommonInfoMultiMap>& get_common_info_multi_map() const {
    return get_info<CommonInfoMultiMap>();
  }
  const absl::optional<CommonInfoMultiIntList>& get_common_info_multi_int_list() const {
    return get_info<CommonInfoMultiIntList>();
  }
  const absl::optional<CommonInfoFixed>& get_common_info_fixed() const {
    return get_info<CommonInfoFixed>();
  }
  const absl::optional<CommonInfoFixedList>& get_common_info_fixed_list() const {
    return get_info<CommonInfoFixedList>();
  }
  const absl::optional<CommonInfoPrepare>& get_common_info_prepare() const {
    return get_info<CommonInfoPrepare>();
  }
  const absl::optional<MiddleNodeInfo>& get_middle_node_info() const {
    return get_info<MiddleNodeInfo>();
  }
  const absl::optional<SeqListInfo>& get_seq_list_info() const { return get_info<SeqListInfo>(); }
  const absl::optional<ProtoListInfo>& get_proto_list_info() const { return get_info<ProtoListInfo>(); }
  const absl::optional<BSFieldInfo>& get_bs_field_info() const { return get_info<BSFieldInfo>(); }

  absl::optional<IfInfo>& mutable_if_info() { return get_info<IfInfo>(); }
  absl::optional<LoopInfo>& mutable_loop_info() { return get_info<LoopInfo>(); }
  absl::optional<SwitchCaseInfo>& mutable_switch_case_info() { return get_info<SwitchCaseInfo>(); }
  absl::optional<DeclInfo>& mutable_decl_info() { return get_info<DeclInfo>(); }
  absl::optional<BinaryOpInfo>& mutable_binary_op_info() { return get_info<BinaryOpInfo>(); }
  absl::optional<AssignInfo>& mutable_assign_info() { return get_info<AssignInfo>(); }
  absl::optional<ActionDetailInfo>& mutable_action_detail_info() { return get_info<ActionDetailInfo>(); }
  absl::optional<ActionDetailFixedInfo>& mutable_action_detail_fixed_info() {
    return get_info<ActionDetailFixedInfo>();
  }
  absl::optional<CommonInfoNormal>& mutable_common_info_normal() {
    return get_info<CommonInfoNormal>();
  }
  absl::optional<CommonInfoMultiMap>& mutable_common_info_multi_map() {
    return get_info<CommonInfoMultiMap>();
  }
  absl::optional<CommonInfoMultiIntList>& mutable_common_info_multi_int_list() {
    return get_info<CommonInfoMultiIntList>();
  }
  absl::optional<CommonInfoFixed>& mutable_common_info_fixed() {
    return get_info<CommonInfoFixed>();
  }
  absl::optional<CommonInfoFixedList>& mutable_common_info_fixed_list() {
    return get_info<CommonInfoFixedList>();
  }
  absl::optional<CommonInfoPrepare>& mutable_common_info_prepare() {
    return get_info<CommonInfoPrepare>();
  }
  absl::optional<MiddleNodeInfo>& mutable_middle_node_info() {
    return get_info<MiddleNodeInfo>();
  }
  absl::optional<SeqListInfo>& mutable_seq_list_info() { return get_info<SeqListInfo>(); }
  absl::optional<ProtoListInfo>& mutable_proto_list_info() { return get_info<ProtoListInfo>(); }
  absl::optional<BSFieldInfo>& mutable_bs_field_info() { return get_info<BSFieldInfo>(); }

  const ConstructorInfo* cur_constructor_info() const { return constructor_info_; }
  const FeatureInfo* cur_feature_info() const { return feature_info_; }
  ConstructorInfo* cur_mutable_constructor_info() { return constructor_info_; }
  FeatureInfo* cur_mutable_feature_info() { return feature_info_; }
  ConstructorInfo* cur_info(const ConstructorInfo& v) { return constructor_info_; }
  FeatureInfo* cur_info(const FeatureInfo& v) { return feature_info_; }
  const ConstructorInfo* cur_info(const ConstructorInfo& v) const { return constructor_info_; }
  const FeatureInfo* cur_info(const FeatureInfo& v) const { return feature_info_; }

  /// 获取 `ConstructorInfo` 指针。
  const ConstructorInfo* get_constructor_info() const;

  /// 获取 `FeatureInfo` 指针。
  const FeatureInfo* get_feature_info() const;

  /// 获取 `ConstructorInfo` 指针。
  ConstructorInfo* mutable_constructor_info();

  /// 获取 `FeatureInfo` 指针。
  FeatureInfo* mutable_feature_info();

  /// 更新变量相关信息。
  void update(clang::DeclStmt* decl_stmt);
  void update(clang::IfStmt* if_stmt);
  void update(clang::ForStmt* for_stmt);
  void update(clang::CXXForRangeStmt* cxx_for_range_stmt);
  void update(clang::BinaryOperator* binary_operator);
  void update(clang::CXXOperatorCallExpr* cxx_operator_call_expr);
  void update(clang::CaseStmt* case_stmt);

  void update_assign_info(clang::BinaryOperator* binary_operator);

  /// 清空 `Decl` 信息。
  void clear_decl_info() { decl_info_ = absl::nullopt; }

  /// 清空 `BinaryOperator` 信息。
  void clear_binary_op_info() {binary_op_info_ = absl::nullopt;}

  /// 清空 `switch case` 信息。
  void clear_switch_case_info() { switch_case_info_ = absl::nullopt; }

  /// 清空 `AssignInfo` 信息。
  void clear_assign_info() { assign_info_ = absl::nullopt; }

  bool is_decl_in_parent_env(const std::string& var_name) const;
  bool is_decl_in_cur_env(const std::string& var_name) const;

  bool is_bslog_field_var_decl(const std::string& var_name) const;

  /// 找到包含具体 var_name 的 map_bs_field_detail。
  std::unordered_map<std::string, BSFieldDetail>*

  find_bs_field_detail_ptr_by_var_name(const std::string& var_name);

  bool is_in_parent_else() const;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
