#include <algorithm>

#include <glog/logging.h>
#include <gflags/gflags.h>
#include <absl/strings/str_split.h>
#include <absl/strings/str_join.h>

#include "Env.h"
#include "Tool.h"
#include "info/CommonInfoMultiIntList.h"
#include "info/IfInfo.h"
#include "info/LoopInfo.h"
#include "info/NewVarDef.h"
#include "info/AdlogFieldInfo.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

const Env* Env::get_loop_parent() const {
  if (is_loop_) {
    return parent_;
  }

  if (parent_ != nullptr) {
    return parent_->get_loop_parent();
  }

  return nullptr;
}

Env* Env::mutable_loop_parent() {
  const Env* loop_parent = get_loop_parent();
  return const_cast<Env*>(loop_parent);
}

const Env* Env::get_loop_env() const {
  if (is_loop_) {
    return this;
  }

  if (parent_ != nullptr) {
    return parent_->get_loop_env();
  }

  return nullptr;
}

const Env *Env::get_outer_loop() const {
  if (is_loop_) {
    if (parent_ != nullptr) {
      if (parent_->is_loop()) {
        return parent_->get_outer_loop();
      } else {
        return this;
      }
    } else {
      return this;
    }
  } else {
    if (parent_ != nullptr) {
      return parent_->get_outer_loop();
    } else {
      return nullptr;
    }
  }
}

Env *Env::mutable_outer_loop() {
  const Env* loop_env = get_outer_loop();
  return const_cast<Env*>(loop_env);
}

const Env *Env::get_outer_loop_parent() const {
  const Env* loop_env = get_outer_loop();

  if (loop_env != nullptr) {
    return loop_env->parent();
  } else {
    return nullptr;
  }
}

Env *Env::mutable_outer_loop_parent() {
  const Env* env_ptr = get_outer_loop_parent();
  return const_cast<Env*>(env_ptr);
}

const Env *Env::get_outer_if() const {
  if (is_if_) {
    if (parent_ != nullptr) {
      if (parent_->is_if()) {
        return parent_->get_outer_if();
      } else {
        return this;
      }
    } else {
      return this;
    }
  } else {
    if (parent_ != nullptr) {
      return parent_->get_outer_if();
    } else {
      return nullptr;
    }
  }
}

Env *Env::mutable_outer_if() {
  const Env *if_env = get_outer_if();
  return const_cast<Env *>(if_env);
}

const Env *Env::get_outer_if_parent() const {
  const Env *if_env = get_outer_if();

  if (if_env != nullptr) {
    return if_env->parent();
  } else {
    return nullptr;
  }
}

Env *Env::mutable_outer_if_parent() {
  const Env *env_ptr = get_outer_if_parent();
  return const_cast<Env *>(env_ptr);
}

Env* Env::mutable_loop_env() {
  const Env* loop_env = get_loop_env();
  return const_cast<Env*>(loop_env);
}

const Env* Env::get_root() const {
  if (parent_ == nullptr) {
    return const_cast<const Env*>(this);
  }

  return parent_->get_root();
}

Env* Env::get_mutable_root() {
  if (parent_ == nullptr) {
    return this;
  }

  return parent_->get_mutable_root();
}

const Env* Env::get_common_info_prepare_env() const {
  if (common_info_prepare_) {
    return this;
  }

  if (parent_ != nullptr) {
    return parent_->get_common_info_prepare_env();
  }

  return nullptr;
}

Env* Env::mutable_common_info_prepare_env() {
  const Env* prepare_env = get_common_info_prepare_env();
  return const_cast<Env*>(prepare_env);
}

const Env* Env::get_common_info_parent_env() const {
  const Env* prepare_env = get_common_info_prepare_env();
  if (prepare_env == nullptr) {
    return nullptr;
  }

  return prepare_env->parent();
}

Env* Env::mutable_common_info_parent_env() {
  Env* prepare_env = mutable_common_info_prepare_env();
  if (prepare_env == nullptr) {
    return nullptr;
  }

  return prepare_env->parent();
}

bool Env::is_parent_loop() const {
  return parent_ != nullptr && parent_->is_loop();
}

bool Env::is_parent_if() const {
  return parent_ != nullptr && parent_->is_if();
}

void Env::add_used_var_name(const std::string& name) {
  used_var_names_.insert(name);
}

bool Env::is_var_name_used(const std::string& name) const {
  return used_var_names_.find(name) != used_var_names_.end();
}

void Env::add_template_var_names(const std::vector<std::string>& var_names) {
  for (size_t i = 0; i < var_names.size(); i++) {
    LOG(INFO) << "add_template_var_name: " << var_names[i];
    used_var_names_.insert(var_names[i]);
  }
}

void Env::add_child(Env* child) {
  if (child != nullptr) {
    children_.push_back(child);
  }
}

void Env::add(const std::string& key, clang::Expr* expr) {
  if (var_decls_.find(key) != var_decls_.end()) {
    LOG(INFO) << "override key: " << key << ", expr: " << stmt_to_string(expr);
  }
  get_mutable_root()->add_used_var_name(key);
  var_decls_[key] = expr;
}

void Env::set_feature_name(const std::string& feature_name) {
  feature_name_ = feature_name;
}

const std::string& Env::feature_name() const {
  const FeatureInfo* feature_info = get_feature_info();
  if (feature_info != nullptr) {
    return feature_info->feature_name();
  }

  static std::string empty;
  return empty;
}

void Env::set_feature_type(const std::string& feature_type) {
  feature_type_ = feature_type;
}

const std::string& Env::feature_type() const {
  const FeatureInfo* feature_info = get_feature_info();
  if (feature_info != nullptr) {
    return feature_info->feature_type();
  }

  static std::string empty;
  return empty;
}

clang::Expr* Env::find(const std::string& key) const {
  auto it = var_decls_.find(key);
  if (it != var_decls_.end()) {
    return it->second;
  }

  if (parent_ != nullptr) {
    return parent_->find(key);
  }

  return nullptr;
}

void Env::erase(const std::string& key) {
  if (var_decls_.find(key) != var_decls_.end()) {
    var_decls_.erase(key);
  }
}

void Env::add_loop_var(const std::string& key) {
  loop_var_names_.push_back(key);
  if (loop_info_) {
    loop_info_->set_loop_var(key);
  }
}

void Env::pop_loop_var() {
  if (loop_var_names_.size() > 0) {
    loop_var_names_.pop_back();
  }
}

const std::string& Env::get_last_loop_var() const {
  if (is_loop_) {
    if (loop_var_names_.size() > 0) {
      return loop_var_names_.back();
    }
  }

  if (parent_ != nullptr) {
    return parent_->get_last_loop_var();
  }

  static std::string empty;
  return empty;
}

bool Env::is_loop_var(const std::string& key) const {
  for (size_t i = 0; i < loop_var_names_.size(); i++) {
    if (loop_var_names_[i] == key) {
      return true;
    }
  }

  if (parent_ != nullptr) {
    return parent_->is_loop_var(key);
  }

  return false;
}

bool Env::is_in_loop() const {
  if (loop_info_) {
    return true;
  }

  if (parent_ != nullptr) {
    return parent_->is_in_loop();
  }

  return false;
}

void Env::add_loop_expr(clang::Expr* expr) {
  if (expr == nullptr) {
    return;
  }

  loop_exprs_.push_back(expr);
}

size_t Env::get_loop_expr_size() {
  return loop_exprs_.size();
}

clang::Expr* Env::get_loop_expr(size_t index) {
  if (index >= loop_exprs_.size()) {
    return nullptr;
  }

  return loop_exprs_[index];
}

clang::Expr* Env::get_first_loop_expr() {
  if (loop_exprs_.size() == 0) {
    return nullptr;
  }

  return loop_exprs_[0];
}

clang::Expr* Env::find_parent_loop() {
  if (parent_ == nullptr) {
    return nullptr;
  }

  return parent_->get_first_loop_expr();
}

bool Env::is_in_if() const {
  if (is_if_) {
    return true;
  }

  if (parent_ != nullptr) {
    return parent_->is_in_if();
  }

  return false;
}

bool Env::is_in_if_cond() const {
  if (is_if_) {
    if (if_info_.has_value() && if_info_->if_stage() == IfStage::COND) {
      return true;
    }
  }

  if (parent_ != nullptr) {
    return parent_->is_in_if_cond();
  }

  return false;
}

bool Env::is_in_if_body() const {
  if (is_if_) {
    if (if_info_.has_value() && if_info_->if_stage() == IfStage::THEN) {
      return true;
    }
  }

  if (parent_ != nullptr) {
    return parent_->is_in_if_body();
  }

  return false;
}

void Env::set_is_if(bool is_if) {
  is_if_ = is_if;
  if (parent_ != nullptr) {
    parent_->set_has_if_in_children(true);
  }
}

bool Env::has_if_in_children() const {
  return has_if_in_children_;
}

void Env::set_has_if_in_children(bool v) {
  has_if_in_children_ = v;
}

bool Env::is_check_middle_node_root_cond() const {
  return if_info_ && if_info_->is_check_middle_node_root_cond();
}

bool Env::is_combine_feature() const {
  if (const auto& feature_info = get_feature_info()) {
    return feature_info->is_combine();
  }

  return false;
}

bool Env::is_user_feature() const {
  return tool::is_user_feature(feature_type());
}

bool Env::is_item_feature() const {
  return tool::is_item_feature(feature_type());
}

bool Env::is_sparse_feature() const {
  return tool::is_sparse_feature(feature_type());
}

bool Env::is_dense_feature() const {
  return tool::is_dense_feature(feature_type());
}

bool Env::is_common_info_loop() const {
  if (loop_info_) {
    return loop_info_->is_common_info_list_map();
  }

  return false;
}

bool Env::is_parent_common_info_loop() const {
  if (parent_ != nullptr) {
    return parent_->is_common_info_loop();
  }

  return false;
}

bool Env::is_in_common_info_loop_body() const {
  if (const Env* prepare_env = get_common_info_prepare_env()) {
    if (const auto& loop_info = prepare_env->cur_loop_info()) {
      if (loop_info->loop_stage() == LoopStage::BODY) {
        return true;
      }
    }
  }

  return false;
}

bool Env::is_in_common_info_if_body() const {
  if (const auto& if_info = cur_if_info()) {
    if (if_info->is_check_common_info_cond()) {
      if (if_info->if_stage() == IfStage::THEN || if_info->if_stage() == IfStage::ELSE) {
        return true;
      }
    }
  }

  if (parent_ != nullptr) {
    return parent_->is_in_common_info_if_body();
  }

  return false;
}

void Env::set_is_child_common_attr_cond(bool v) {
  is_child_common_attr_cond_ = v;
}

bool Env::is_child_common_attr_cond() const {
  return is_child_common_attr_cond_;
}

absl::optional<int> Env::get_common_attr_int_value() const {
  if (const auto& common_info_normal = get_common_info_normal()) {
    if (if_info_) {
      if (const absl::optional<int>& index = if_info_->common_info_index()) {
        if (*index < common_info_normal->common_info_details_size()) {
          return absl::optional<int>(common_info_normal->get_common_info_detail(*index)->common_info_value());
        } else {
          LOG(INFO) << "index out of range for common info details, index: " << *index
                    << ", common_info_details.size: " <<  common_info_normal->common_info_details_size();
          return absl::nullopt;
        }
      }
    } else {
      // 通过 != 来判断, if_info 已不存在, 只能通过 last_common_info_detail 获取
      if (common_info_normal->common_info_details_size() > 0) {
        const auto& last_common_info_detail = common_info_normal->last_common_info_detail();
        return absl::optional<int>(last_common_info_detail->common_info_value());
      }
    }
  }

  if (parent_ != nullptr) {
    return parent_->get_common_attr_int_value();
  }

  return absl::nullopt;
}

absl::optional<std::string> Env::get_common_attr_int_name() const {
  if (const auto& common_info_fixed_list = get_common_info_fixed_list()) {
    if (if_info_) {
      if (const auto last_detail = common_info_fixed_list->last_common_info_detail()) {
        return absl::optional<std::string>(last_detail->int_name());
      }
    }
  }

  if (parent_ != nullptr) {
    return parent_->get_common_attr_int_name();
  }

  return absl::nullopt;
}

bool Env::is_check_item_pos_cond() const {
  return is_if_ && if_info_->is_check_item_pos_cond();
}

bool Env::is_action_detail_cond() const {
  return is_if_ && action_detail_info_.has_value();
}

void Env::set_cxx_for_range_stmt(clang::CXXForRangeStmt* cxx_for_range_stmt) {
  cxx_for_range_stmt_ = cxx_for_range_stmt;
}
clang::CXXForRangeStmt* Env::cxx_for_range_stmt() const {
  return cxx_for_range_stmt_;
}

void Env::set_for_stmt(clang::ForStmt* for_stmt) {
  for_stmt_ = for_stmt;
}

clang::ForStmt* Env::for_stmt() const {
  return for_stmt_;
}

void Env::set_if_stmt(clang::IfStmt* if_stmt) {
  if_stmt_ = if_stmt;
}

clang::IfStmt* Env::if_stmt() const {
  if (if_stmt_ != nullptr) {
    return if_stmt_;
  }

  if (parent_ != nullptr) {
    return parent_->if_stmt();
  }

  return nullptr;
}

// 如果 if 里的语句都是变量定义，并且这些变量最终都被删掉，那么整条 if 语句可以被删掉。
bool Env::is_all_if_stmt_deleted() const {
  if (if_stmt_ == nullptr) {
    return false;
  }

  int total_deleted = deleted_vars_.size();
  if (total_deleted == 0) {
    return false;
  }

  int total_if_stmt = 0;
  if (clang::CompoundStmt* compound_stmt = dyn_cast<clang::CompoundStmt>(if_stmt_->getThen())) {
    total_if_stmt = compound_stmt->size();
  }

  return total_deleted == total_if_stmt;
}

void Env::add_decl_stmt(const std::string& name, clang::DeclStmt* decl_stmt) {
  decl_stmts_[name] = decl_stmt;
  get_mutable_root()->add_used_var_name(name);
}

clang::DeclStmt* Env::get_decl_stmt(const std::string& name) const {
  auto it = decl_stmts_.find(name);
  if (it != decl_stmts_.end()) {
    return it->second;
  }

  if (parent_ != nullptr) {
    return parent_->get_decl_stmt(name);
  }

  return nullptr;
}

clang::DeclStmt* Env::get_decl_stmt_in_cur_env(const std::string& name) const {
  auto it = decl_stmts_.find(name);
  if (it != decl_stmts_.end()) {
    return it->second;
  }

  return nullptr;
}

Env* Env::find_decl_env(const std::string& name) {
  if (decl_stmts_.find(name) != decl_stmts_.end()) {
    return this;
  }

  if (parent_ != nullptr) {
    return parent_->find_decl_env(name);
  }

  return nullptr;
}

void Env::add_deleted_var(const std::string& name) {
  LOG(INFO) << "add deleted var: " << name;
  deleted_vars_.insert(name);
}

// 可能有隐式转换, 统一通过 str 来判断
bool Env::add_deleted_var_by_expr(clang::Expr* expr) {
  for (auto it = var_decls_.begin(); it != var_decls_.end(); it++) {
    if (stmt_to_string(expr) == stmt_to_string(it->second)) {
      deleted_vars_.insert(it->first);
      return true;
    }
  }

  return false;
}

// 可能有隐式转换, 统一通过 str 来判断
bool Env::add_deleted_var_by_expr_str(const std::string& expr_str) {
  for (auto it = var_decls_.begin(); it != var_decls_.end(); it++) {
    if (expr_str == stmt_to_string(it->second)) {
      deleted_vars_.insert(it->first);
      return true;
    }
  }

  return false;
}

void Env::pop_deleted_var(const std::string& name) {
  deleted_vars_.erase(name);
}

void Env::clear_deleted_var() {
  deleted_vars_.clear();
}

Env* Env::mutable_new_def_target_env(bool is_from_reco) {
  if (is_from_reco) {
    if (parent_ != nullptr) {
      return parent_->mutable_new_def_target_env(is_from_reco);
    } else {
      return this;
    }
  }

  // common info 比较特殊，整个 for 循环会被替换调，必须定义在 commno info prepare parent 中，否则会找不到。
  if (is_in_common_info_loop_body()) {
    return mutable_common_info_parent_env();
  }

  if (is_in_loop()) {
    return mutable_loop_parent();
  }

  if (const auto& if_info = cur_if_info()) {
    if (if_info->is_check_action_detail_cond()) {
      return parent_;
    }
    if (if_info->if_stage() == IfStage::COND) {
      return parent_;
    }
  }

  return this;
}

void Env::add_new_def(const std::string& bs_enum_str,
                      const std::string& var_def,
                      NewVarType new_var_type) {
  bool is_from_reco  = tool::is_str_from_reco_user_info(bs_enum_str);
  Env* target_env = mutable_new_def_target_env(is_from_reco);
  if (target_env == nullptr) {
    return;
  }

  target_env->add_new_def_helper(bs_enum_str, var_def, new_var_type);
}

void Env::add_new_def_helper(const std::string &bs_enum_str,
                             const std::string &var_def,
                             NewVarType new_var_type) {
  absl::optional<NewVarDef> &var = find_mutable_new_def(bs_enum_str);
  if (var) {
    if (var->var_def().size() == 0) {
      var->set_var_def(var_def, new_var_type);
    }

    return;
  }

  std::string var_name = find_valid_new_name(bs_enum_str);
  get_mutable_root()->add_used_var_name(var_name);
  absl::optional<NewVarDef> new_def = absl::make_optional<NewVarDef>(
      bs_enum_str, var_name, var_def, new_var_type);
  new_defs_[bs_enum_str] = std::move(new_def);
}

void Env::add_new_def_meta(const std::string& bs_enum_str,
                           const std::string& var_def,
                           NewVarType new_var_type) {
  add_new_def(bs_enum_str, var_def, new_var_type);
  add_attr_meta(bs_enum_str);
}

void Env::add_attr_meta(const std::string& bs_enum_str) {
  if (starts_with(bs_enum_str, "adlog")) {
    if (auto constructor_info = mutable_constructor_info()) {
      constructor_info->add_bs_field_enum(bs_enum_str);
    }
  } else {
    LOG(INFO) << "bs_enum_str is not starts_with adlog, skip! bs_enum_str: " << bs_enum_str;
  }
}

void Env::set_normal_adlog_field_info(const std::string& bs_enum_str,
                                      const std::string& adlog_field) {
  if (auto construct_info = mutable_constructor_info()) {
    construct_info->set_normal_adlog_field_info(bs_enum_str, adlog_field);
  }
}

void Env::set_common_info_field_info(const std::string& bs_enum_str,
                                     const std::string& adlog_field,
                                     const std::string& common_info_enum_name,
                                     int common_info_value) {
  if (auto construct_info = mutable_constructor_info()) {
    construct_info->set_common_info_field_info(bs_enum_str,
                                               adlog_field,
                                               common_info_enum_name,
                                               common_info_value);
  }
}

// 可能会出现在添加的 exists var 后面，因此也需要先查找是否存在。
// var_name 会用第一次出现时候查找的新名字。即如果 decl_stmt 后出现，并且名字和添加 def 时候的不一样,
// 则需要替换成添加 def 时的名字。如下
// auto iter = ad_action.find(no);
// if (iter != ad_action.end()) {
//   const auto& action_base_infos = iter->second.list();
//   int64_t imp_num = action_base_infos.size();
//   AddFeature(0, imp_num, result);
//  } else {
//   AddFeature(0, 0, result);
//  }
// 在遇到 ad_action.find(no) 时，就知道要判断 action_detail 是否存在, 现在是根据
// action_detail_key_${no}_list_size 是否存在来判断的, 因此现在要添加 action_detail_key_${no}_list_size
// 对应的 exists_def, 因为是第一次添加，因此会找到一个新的 var_name: list_size。但是后面又遇到
// action_base_infos.size() 时, bs_enum_str 是和刚才的一样的, 但是已经有了一个 decl_stmt 的 var_name:
// imp_num。此时就要决定用哪个变量名了。如果用 decl_stmt 中的 decl name, 那么当 decl name 已经被用过时，
// 直接将 decl name 设置为 var_name, 则之后的引用都没问题。但是之前已经替换的引用会有问题，需要想办法解决。
// 不过这种情况出现的比较少, 在 action_detail 判断中可能会遇到。action.find(no) 之后会需要立即判断是否存在,
// 需要用到 key_${no}_list_size 对应的 exists_var, 之后的处理逻辑中可能直接用到 list_size 这个变量, 如
// int64_t imp_num = action_base_infos.size(), 对应的 var_name 是 imp_num。因此只需要不将 var_name 和
// exists_name 耦合即可, 这两个属性都可以手动设置即可满足需求。
//
// loop body 内添加的 adlog var 统一定义到 loop parent env 中，因为 loop 可能需要整体被替换, 防止
// 出现重复定义。
//
// reco_user_info 字段比较特殊，需要替换两次，必须保存在 root env 中。
void Env::add_new_def(const std::string& bs_enum_str,
                      const std::string& var_name,
                      const std::string& var_def,
                      NewVarType new_var_type) {
  bool is_from_reco  = tool::is_str_from_reco_user_info(bs_enum_str);
  Env *target_env = mutable_new_def_target_env(is_from_reco);
  if (target_env == nullptr) {
    return;
  }

  target_env->add_new_def_helper(bs_enum_str, var_name, var_def, new_var_type);
}

void Env::add_new_def_helper(const std::string &bs_enum_str,
                             const std::string &var_name,
                             const std::string &var_def,
                             NewVarType new_var_type) {
  absl::optional<NewVarDef> &var = find_mutable_new_def(bs_enum_str);
  if (var) {
    LOG(INFO) << "var_def already exists, bs_enum_str: " << bs_enum_str
              << ", old var_name: " << var->name()
              << ", set var_name: " << var_name;
    var->set_name(var_name);

    if (var->var_def().size() == 0) {
      var->set_var_def(var_def, new_var_type);
    }

    return;
  }

  absl::optional<NewVarDef> new_def = absl::make_optional<NewVarDef>(
      bs_enum_str, var_name, var_def, new_var_type);
  new_defs_[bs_enum_str] = std::move(new_def);
  get_mutable_root()->add_used_var_name(var_name);
}

void Env::add_new_def_meta(const std::string& bs_enum_str,
                           const std::string& var_name,
                           const std::string& var_def,
                           NewVarType new_var_type) {
  add_new_def(bs_enum_str, var_name, var_def, new_var_type);
  add_attr_meta(bs_enum_str);
}

void Env::add_new_exists_def(const std::string &bs_enum_str,
                             const std::string &exists_var_def) {
  bool is_from_reco  = tool::is_str_from_reco_user_info(bs_enum_str);
  if (auto env = mutable_new_def_target_env(is_from_reco)) {
    env->add_new_exists_def_helper(bs_enum_str, exists_var_def);
  }
}

void Env::add_new_exists_def_helper(const std::string& bs_enum_str,
                                     const std::string& exists_var_def) {
  if (absl::optional<NewVarDef>& var_def = find_mutable_new_def(bs_enum_str)) {
    var_def->set_exists_var_def(tool::get_exists_name(var_def->name()), exists_var_def);
  } else {
    std::string var_name = find_valid_new_name(bs_enum_str);
    get_mutable_root()->add_used_var_name(var_name);

    absl::optional<NewVarDef> new_def = absl::make_optional<NewVarDef>(bs_enum_str, var_name);
    std::string exists_name = tool::get_exists_name(var_name);
    new_def->set_exists_var_def(exists_name, exists_var_def);
    new_defs_.emplace(bs_enum_str, new_def);
  }
}

void Env::add_new_exists_def_meta(const std::string& bs_enum_str,
                                  const std::string& exists_var_def) {
  add_new_exists_def(bs_enum_str, exists_var_def);
  add_attr_meta(bs_enum_str);
}

const absl::optional<NewVarDef>& Env::find_new_def(const std::string& bs_enum_str) const {
  auto it = new_defs_.find(bs_enum_str);
  if (it != new_defs_.end() && it->second.has_value()) {
    return it->second;
  }

  if (parent_ != nullptr) {
    return parent_->find_new_def(bs_enum_str);
  }

  static absl::optional<NewVarDef> empty = absl::nullopt;
  return empty;
}

void Env::add_new_def_overwrite(
  const std::string &bs_var_name,
  const std::string &var_def,
  NewVarType new_var_type
) {
  absl::optional<NewVarDef> &var = find_mutable_new_def(bs_var_name);
  if (var) {
    if (var->var_def().size() == 0) {
      var->set_var_def(var_def, new_var_type);
    }

    return;
  }

  get_mutable_root()->add_used_var_name(bs_var_name);
  absl::optional<NewVarDef> new_def = absl::make_optional<NewVarDef>(
      bs_var_name, bs_var_name, var_def, new_var_type);
  new_defs_[bs_var_name] = std::move(new_def);
}

absl::optional<NewVarDef>& Env::find_mutable_new_def(const std::string& bs_enum_str) {
  const absl::optional<NewVarDef>& var_def = find_new_def(bs_enum_str);
  return const_cast<absl::optional<NewVarDef>&>(var_def);
}

std::string Env::find_valid_new_name(const std::string& bs_enum_str) const {
  const absl::optional<NewVarDef>& var_def = find_new_def(bs_enum_str);
  if (var_def) {
    // var_def 第一次添加的时候一定有合法的 name。
    return var_def->name();
  }

  std::vector<std::string> tokens = absl::StrSplit(bs_enum_str, "_");

  if (tokens.size() == 1) {
    if (is_new_name_valid(tokens[0])) {
      return tokens[0];
    }
  }

  for (int i = tokens.size() - 2; i >= 0; i--) {
    if (tokens[i].size() > 0 && std::isdigit(tokens[i][0])) {
      continue;
    }
    std::string new_name = absl::StrJoin(tokens.begin() + i, tokens.end(), "_");
    if (is_new_name_valid(new_name)) {
      return new_name;
    }
  }

  LOG(INFO) << "cannot find valid name, bs_enum_str is too short: " << bs_enum_str;
  return "";
}

bool Env::is_new_name_valid(const std::string& name) const {
  if (get_root()->is_var_name_used(name)) {
    return false;
  }

  return true;
}

bool Env::is_new_var_exists(const std::string& bs_enum_str) const {
  const absl::optional<NewVarDef>& var_def = find_new_def(bs_enum_str);
  if (var_def && var_def->var_def().size() > 0) {
    return true;
  }

  return false;
}

bool Env::is_new_var_not_exists(const std::string& bs_enum_str) const {
  return !is_new_var_exists(bs_enum_str);
}

const std::string& Env::find_new_var_name(const std::string& bs_enum_str) const {
  if (const absl::optional<NewVarDef>& var_def = find_new_def(bs_enum_str)) {
    return var_def->name();
  }

  static std::string empty;
  return empty;
}

const std::string& Env::find_new_exists_var_name(const std::string& bs_enum_str) const {
  if (const absl::optional<NewVarDef>& var_def = find_new_def(bs_enum_str)) {
    return var_def->exists_name();
  }

  static std::string empty;
  return empty;
}

std::vector<std::string> Env::get_all_new_def_var_names() const {
  std::vector<std::string> res;

  for (auto it = new_defs_.begin(); it != new_defs_.end(); it++) {
    if (it->second.has_value()) {
      res.emplace_back(it->second->name());
    }
  }

  return res;
}

std::string Env::get_all_new_defs() const {
  std::ostringstream oss;
  for (auto it = new_defs_.begin(); it != new_defs_.end(); it++) {
    if (it->second.has_value()) {
      LOG(INFO) << "new var, var_name: " << it->second->name() << ", def: " << it->second->var_def();
      if (it->second->var_def().size() > 0) {
        oss << it->second->var_def() << ";\n\n";
      }
      if (it->second->exists_var_def().size() > 0) {
        oss << it->second->exists_var_def() << ";\n\n";
      }
    }
  }

  return oss.str();
}

int Env::get_action() {
  if (action_ != -1) {
    return action_;
  }

  if (parent_ != nullptr) {
    return parent_->get_action();
  }

  return -1;
}

void Env::set_action_expr(clang::Expr* expr, std::string bs_action_expr) {
  action_expr_ = expr;
  bs_action_expr_ = bs_action_expr;
}

std::string Env::get_bs_action_expr() {
  if (bs_action_expr_.size() > 0) {
    return bs_action_expr_;
  }

  if (parent_ != nullptr) {
    return parent_->get_bs_action_expr();
  }

  return "";
}

std::string Env::find_one_action_detail_leaf_name(const std::string& bs_enum_str) const {
  for (auto it = new_defs_.begin(); it != new_defs_.end(); it++) {
    if (starts_with(it->first, bs_enum_str)) {
      const absl::optional<NewVarDef>& new_var_def = it->second;
      if (new_var_def && new_var_def->is_list()) {
        return new_var_def->name();
      }
    }
  }

  if (parent_ != nullptr) {
    return parent_->find_one_action_detail_leaf_name(bs_enum_str);
  }

  return "";
}

void Env::add_if_stmt(clang::IfStmt* if_stmt) {
  if_info_.emplace(if_stmt);
}

void Env::add_loop_stmt(clang::ForStmt* for_stmt) {
  loop_info_.emplace(for_stmt);
}

void Env::add_loop_stmt(clang::CXXForRangeStmt* cxx_for_range_stmt) {
  loop_info_.emplace(cxx_for_range_stmt);
}

void Env::add_action_detail_prefix_adlog(const std::string& prefix_adlog) {
  action_detail_prefix_adlog_.emplace(prefix_adlog);
}

// 多个 common info
// 一定要找到 prefix 所在 loop Env 来创建，才能保证唯一。
absl::optional<CommonInfoNormal>& Env::touch_common_info_normal() {
  static absl::optional<CommonInfoNormal> empty;

  if (common_info_prepare_ && common_info_prepare_->prefix()) {
    if (!common_info_prepare_->is_common_info_normal()) {
      return empty;
    }

    if (!common_info_normal_) {
      const std::string& prefix_adlog = *(common_info_prepare_->prefix_adlog());
      if (starts_with(prefix_adlog, "adlog") || (feature_name() == "ItemFilter" && starts_with(prefix_adlog, "item"))) {
        LOG(INFO) << "touch common_info_normal, prefix_adlog: "
                  << *(common_info_prepare_->prefix_adlog());
        common_info_normal_.emplace(*(common_info_prepare_->prefix_adlog()));
      } else if (const auto &middle_node_info = get_middle_node_info()) {
        LOG(INFO) << "touch common_info_normal with middle_node, middle_node: "
                  << middle_node_info->name()
                  << ", prefix_adlog: " << *(common_info_prepare_->prefix_adlog());
        common_info_normal_.emplace(*(common_info_prepare_->prefix_adlog()), middle_node_info->name());
      } else {
        LOG(INFO) << "prefix_adlog is not starts_with adlog! prefix_adlog: " << prefix_adlog;
        return empty;
      }

      common_info_normal_->set_env_ptr(this);
      if (common_info_prepare_->name_value_alias()) {
        common_info_normal_->set_name_value_alias(*(common_info_prepare_->name_value_alias()));
      }
      common_info_prepare_->set_is_confirmed();
    }

    return common_info_normal_;
  }

  if (parent_ != nullptr) {
    return parent_->touch_common_info_normal();
  }

  LOG(INFO) << "canot find common info prefix!";
  return empty;
}

// common info enum 变量通过模板参数传递
absl::optional<CommonInfoFixedList>& Env::touch_common_info_fixed_list() {
  static absl::optional<CommonInfoFixedList> empty;

  if (common_info_prepare_ && common_info_prepare_->prefix_adlog()) {
    // if (common_info_prepare_->is_common_info_normal()) {
    //   return (empty);
    // }
    if (!common_info_fixed_list_) {
      const std::string& prefix_adlog = *(common_info_prepare_->prefix_adlog());
      if (starts_with(prefix_adlog, "adlog")
          || (feature_name() == "ItemFilter"
              && starts_with(prefix_adlog, "item"))) {
        common_info_fixed_list_.emplace(*(common_info_prepare_->prefix_adlog()));
        LOG(INFO) << "touch common_info_fixed_list, prefix_adlog: "
                  << *(common_info_prepare_->prefix_adlog());
      } else if (const auto& middle_node_info = get_middle_node_info()) {
        common_info_fixed_list_.emplace(*(common_info_prepare_->prefix_adlog()),
                                        middle_node_info);
        LOG(INFO) << "touch common_info_fixed_list with middle_node, prefix_adlog: "
                  << *(common_info_prepare_->prefix_adlog())
                  << ", middle_node root: " << middle_node_info->name();
      } else {
        LOG(INFO) << "prefix_adlog is not starts_with adlog! prefix_adlog: "
                  << prefix_adlog;
        return empty;
      }

      common_info_fixed_list_->set_env_ptr(this);
      common_info_prepare_->set_is_confirmed();
    }
    return common_info_fixed_list_;
  }

  if (parent_ != nullptr) {
    return parent_->touch_common_info_fixed_list();
  }

  LOG(INFO) << "canot find common info prefix!";
  return empty;
}

// 多个 common info
// 一定要找到 prefix 所在 loop Env 来创建，才能保证唯一。
absl::optional<CommonInfoMultiMap>& Env::touch_common_info_multi_map(const std::string& map_name,
                                                                     const std::string& attr_name) {
  if (common_info_prepare_ && common_info_prepare_->prefix()) {
    if (!common_info_multi_map_) {
      common_info_multi_map_.emplace(*(common_info_prepare_->prefix_adlog()), map_name, attr_name);
      common_info_multi_map_->set_env_ptr(this);
      common_info_prepare_->set_is_confirmed();
      LOG(INFO) << "touch common_info_multi_map, prefix_adlog: "
                << *(common_info_prepare_->prefix_adlog());
    }
    return (common_info_multi_map_);
  }

  if (parent_ != nullptr) {
    return parent_->touch_common_info_multi_map(map_name, attr_name);
  }

  LOG(INFO) << "cannot get common info prefix for multi map!";
  static absl::optional<CommonInfoMultiMap> empty;
  return empty;
}

absl::optional<CommonInfoMultiIntList>& Env::touch_common_info_multi_int_list() {
  if (common_info_prepare_ && common_info_prepare_->prefix()) {
    if (!common_info_multi_int_list_) {
      common_info_multi_int_list_.emplace(*(common_info_prepare_->prefix_adlog()));
      common_info_multi_int_list_->set_env_ptr(this);
      common_info_prepare_->set_is_confirmed();
      LOG(INFO) << "touch common_info_multi_int_list, prefix_adlog: "
                << *(common_info_prepare_->prefix_adlog());

      // 从 feature info 中复制 map_vec_connections
      if (auto feature_info = mutable_feature_info()) {
        if (const auto& int_list_info = feature_info->common_info_multi_int_list()) {
          LOG(INFO) << "int_list_info address: " << &(*int_list_info);
          const auto& map_vec_connections = int_list_info->map_vec_connections();
          for (auto it = map_vec_connections.begin(); it != map_vec_connections.end(); it++) {
            common_info_multi_int_list_->add_map_vec_connection(it->first, it->second);
            LOG(INFO) << "copy from feature_info, map_name: " << it->first
                      << ", vec_name: " << it->second;
          }
        }
      }
    }

    // 目前还区分不了
    if (common_info_fixed_list_) {
      common_info_fixed_list_ = absl::nullopt;
    }

    return (common_info_multi_int_list_);
  }

  if (parent_ != nullptr) {
    return parent_->touch_common_info_multi_int_list();
  }

  LOG(INFO) << "cannot get common info prefix for multi_int_list!";
  static absl::optional<CommonInfoMultiIntList> empty;
  return empty;
}

absl::optional<ActionDetailInfo>& Env::touch_action_detail_info(int action) {
  if (action_detail_prefix_adlog_) {
    if (!action_detail_info_) {
      action_detail_info_.emplace(*action_detail_prefix_adlog_, action);
      action_detail_info_->set_env_ptr(this);
    }

    return (action_detail_info_);
  }

  if (parent_ != nullptr) {
    return parent_->touch_action_detail_info(action);
  }

  LOG(INFO) << "cannot find action detail prefix!";
  static absl::optional<ActionDetailInfo> empty;
  return empty;
}

absl::optional<ActionDetailInfo>& Env::update_action_detail_info(int action) {
  if (auto& action_detail_info = touch_action_detail_info(action)) {
    action_detail_info->add_action(action);
    return action_detail_info;
  }

  LOG(INFO) << "update_action_detail_info faield! cannot find action detail prefix!";
  static absl::optional<ActionDetailInfo> empty;
  return empty;
}

absl::optional<ActionDetailFixedInfo>& Env::touch_action_detail_fixed_info(const std::string& action) {
  if (action_detail_prefix_adlog_) {
    if (!action_detail_fixed_info_) {
      action_detail_fixed_info_.emplace(*action_detail_prefix_adlog_, action);
      action_detail_fixed_info_->set_env_ptr(this);
    }

    return action_detail_fixed_info_;
  }

  if (parent_ != nullptr) {
    return parent_->touch_action_detail_fixed_info(action);
  }

  LOG(INFO) << "cannot find action detail prefix!";
  static absl::optional<ActionDetailFixedInfo> empty;
  return empty;
}

absl::optional<SeqListInfo>& Env::touch_seq_list_info(const std::string& root_name) {
  if (!seq_list_info_) {
    seq_list_info_.emplace(root_name);
  }

  return seq_list_info_;
}

absl::optional<ProtoListInfo> & Env::touch_proto_list_info(const std::string &prefix_adlog) {
  if (!proto_list_info_) {
    proto_list_info_.emplace(prefix_adlog);
  }

  return proto_list_info_;
}

absl::optional<BSFieldInfo>& Env::touch_bs_field_info(const std::string& bs_var_name) {
  if (decl_stmts_.find(bs_var_name) != decl_stmts_.end()) {
    if (!bs_field_info_) {
      bs_field_info_.emplace();
    }

    return bs_field_info_;
  }

  if (parent_ != nullptr) {
    return parent_->touch_bs_field_info(bs_var_name);
  }

  return bs_field_info_;
}

void Env::add_middle_node_name(const std::string& name) {
  middle_node_info_.emplace(name);
}

void Env::set_feature_info(FeatureInfo* feature_info) {
  feature_info_ = feature_info;
}

void Env::set_constructor_info(ConstructorInfo* constructor_info) {
  constructor_info_ = constructor_info;
}

void Env::set_common_info_prefix_adlog(const std::string& prefix_adlog) {
  common_info_prepare_.emplace(prefix_adlog);
  if (auto feature_info = mutable_feature_info()) {
    if (const auto& info_prepare = feature_info->common_info_prepare()) {
      common_info_prepare_->set_template_int_names(info_prepare->template_int_names());
      common_info_prepare_->set_common_info_values(info_prepare->common_info_values());
    }
  }
}

const absl::optional<std::string>& Env::common_info_prefix() const {
  if (common_info_prepare_) {
    return common_info_prepare_->prefix();
  }

  static absl::optional<std::string> empty;
  return empty;
}

const absl::optional<std::string>& Env::get_common_info_prefix() const {
  if (common_info_prepare_) {
    return common_info_prepare_->prefix();
  }

  if (parent_ != nullptr) {
    return parent_->get_common_info_prefix();
  }

  static absl::optional<std::string> empty;
  return empty;
}

void Env::update_template_common_info_values() {
  if (common_info_normal_) {
    if (const auto& feature_info = get_feature_info()) {
      if (Env* parent_env_ptr = common_info_normal_->parent_env_ptr()) {
        if (feature_info->is_template()) {
          const std::set<int>& values = feature_info->template_common_info_values();
          for (int value: values) {
            if (common_info_normal_->is_already_exists(value)) {
              continue;
            }
            common_info_normal_->add_common_info_value(value);
            const auto& common_info_detail = common_info_normal_->last_common_info_detail();
            parent_env_ptr->add_common_info_detail_def(*common_info_detail);
          }
        }
      }
    }
  }
}

void Env::add_common_info_detail_def(const CommonInfoLeaf& common_info_detail) {
  std::string bs_enum_str = common_info_detail.get_bs_enum_str();
  if (bs_enum_str.size() == 0) {
    LOG(INFO) << "cannot get bs_enum_str from common_info_detail!";
    return;
  }

  if (is_new_var_exists(bs_enum_str)) {
    return;
  }

  if (common_info_detail.is_ready()) {
    if (common_info_detail.is_scalar()) {
      // 处理单值, 如 attr.int_value()
      if (is_new_var_not_exists(bs_enum_str)) {
        // 此处只添加单值的 exists 定义, 取值的定义在 basic_scalar 中添加
        LOG(INFO) << "add scalar exists, bs_enum_str: " << bs_enum_str
                  << ", def: " << common_info_detail.get_bs_scalar_exists_def(this);
        add_new_exists_def_meta(bs_enum_str,
                                common_info_detail.get_bs_scalar_exists_def(this));
        add_new_def_meta(bs_enum_str,
                         common_info_detail.get_bs_scalar_def(this),
                         NewVarType::SCALAR);
        if (const auto& enum_name = common_info_detail.common_info_enum_name()) {
          set_common_info_field_info(bs_enum_str,
                                     common_info_detail.get_adlog_field_str(),
                                     *enum_name,
                                     common_info_detail.common_info_value());
        }
      }
    } else {
      // 处理 list 或者 map, 如 attr.int_list_value(i)
      if (common_info_detail.is_list()) {
        LOG(INFO) << "add list def, bs_enum_str: " << bs_enum_str
                  << ", list_def: " << common_info_detail.get_bs_list_def(this);
        add_new_def_meta(bs_enum_str,
                         common_info_detail.get_bs_list_def(this),
                         NewVarType::LIST);
        if (const auto& enum_name = common_info_detail.common_info_enum_name()) {
          LOG(INFO) << "set common_info_field_info, bs_enum_str: " << bs_enum_str
                    << ", enum_name: " << *enum_name;
          set_common_info_field_info(bs_enum_str,
                                     common_info_detail.get_adlog_field_str(),
                                     *enum_name,
                                     common_info_detail.common_info_value());
        }
      } else if (common_info_detail.is_map()) {
        LOG(INFO) << "add map def, bs_enum_str: " << bs_enum_str
                  << ", map_def: " << common_info_detail.get_bs_map_def(this);
        add_new_def(bs_enum_str,
                    common_info_detail.get_bs_map_def(this),
                    NewVarType::MAP);
        add_attr_meta(bs_enum_str + "_key");
        add_attr_meta(bs_enum_str + "_value");

        if (const auto& enum_name = common_info_detail.common_info_enum_name()) {
          set_common_info_field_info(bs_enum_str + std::string("_key"),
                                     common_info_detail.get_adlog_field_str() + std::string(".key"),
                                     *enum_name,
                                     common_info_detail.common_info_value());

          set_common_info_field_info(bs_enum_str + std::string("_value"),
                                     common_info_detail.get_adlog_field_str() + std::string(".value"),
                                     *enum_name,
                                     common_info_detail.common_info_value());
        }
      }
    }

    if (common_info_detail.common_info_type() == CommonInfoType::MIDDLE_NODE) {
      if (auto feature_info = mutable_feature_info()) {
        if (common_info_detail.is_scalar()) {
          feature_info->add_field_def(common_info_detail.get_bs_enum_str(),
                                      common_info_detail.get_functor_name(),
                                      common_info_detail.get_bs_scalar_field_def(this),
                                      common_info_detail.get_exists_functor_name(),
                                      common_info_detail.get_bs_scalar_exists_field_def(this),
                                      NewVarType::SCALAR,
                                      AdlogVarType::COMMON_INFO_MIDDLE_NODE);
        } else if (common_info_detail.is_list() || common_info_detail.is_list_size()) {
          feature_info->add_field_def(common_info_detail.get_bs_enum_str(),
                                      common_info_detail.get_functor_name(),
                                      common_info_detail.get_bs_list_field_def(this),
                                      NewVarType::LIST,
                                      AdlogVarType::COMMON_INFO_MIDDLE_NODE);
        } else if (common_info_detail.is_map() || common_info_detail.is_map_size()) {
          feature_info->add_field_def(common_info_detail.get_bs_enum_str(),
                                      common_info_detail.get_functor_name(),
                                      common_info_detail.get_bs_map_field_def(this),
                                      NewVarType::MAP,
                                      AdlogVarType::COMMON_INFO_MIDDLE_NODE);
        }

        if (const auto& name_value_alias = common_info_detail.name_value_alias()) {
          feature_info->set_common_info_prefix_name_value(common_info_detail.get_bs_enum_str(),
                                                          common_info_detail.prefix_adlog(),
                                                          *name_value_alias);
        }
      }
    }
  }
}

void Env::clear_common_info_fixed_list() {
  if (auto& common_info_fixed_list = mutable_common_info_fixed_list()) {
    common_info_fixed_list = absl::nullopt;
  }
}

void Env::add_ctor_decls(const VarDeclInfo& var_decl_info) {
  const std::unordered_map<std::string, DeclInfo>& var_decls = var_decl_info.var_decls();
  for (auto it = var_decls.begin(); it != var_decls.end(); it++) {
    LOG(INFO) << "add ctor decl, name: " << it->first << ", v: " << stmt_to_string(it->second.init_expr());
    var_decls_.emplace(it->first, it->second.init_expr());
  }
}

void Env::update(clang::DeclStmt* decl_stmt) {
  clang::VarDecl* var_decl = dyn_cast<clang::VarDecl>(decl_stmt->getSingleDecl());
  if (var_decl == nullptr) {
    return;
  }
  LOG(INFO) << "update decl_stmt: " << stmt_to_string(decl_stmt);

  std::string var_name = var_decl->getNameAsString();
  if (!var_decl->hasInit()) {
    add(var_name, nullptr);
    decl_info_.emplace(var_name);
    if (!tool::is_basic_array(var_decl->getType()) && !tool::is_builtin_simple_type(var_decl->getType())) {
      LOG(INFO) << "add_deleted_var: " << var_name;
      add_deleted_var(var_name);
    }
    add_decl_stmt(var_name, decl_stmt);
    return;
  }

  clang::Expr* expr = var_decl->getInit();
  expr = expr->IgnoreCasts();

  if (find(var_name) != nullptr) {
    LOG(INFO) << "overwrite var_name: " << var_name << ", stmt: " << stmt_to_string(expr);
  }

  if (starts_with(var_name, "__range") && var_name.find(".") == std::string::npos) {
    std::string expr_str = stmt_to_string(expr);
    if (ends_with(expr_str, "begin()") || ends_with(expr_str, "end()")) {
      std::vector<std::string> str_arr = absl::StrSplit(expr_str, ".");
      if (str_arr[0] != "adlog") {
        add_deleted_var(str_arr[0]);
        LOG(INFO) << "add_deleted_var: " << str_arr[0];
      }
    }
  }

  add(var_name, expr);
  decl_info_.emplace(var_name, expr, decl_stmt);

  if (stmt_to_string(decl_stmt).find("static") == std::string::npos) {
    add_decl_stmt(var_name, decl_stmt);
  }
}

void Env::update(clang::IfStmt* if_stmt) {
  set_is_if(true);
  set_if_stmt(if_stmt);
  if_info_.emplace(if_stmt);

  if (parent_ != nullptr) {
    parent_->increase_if_index();
  }
}

void Env::update(clang::ForStmt* for_stmt) {
  set_for_stmt(for_stmt);
  set_is_loop(true);
  loop_info_.emplace(for_stmt);
  loop_info_->set_env_ptr(this);
}

void Env::update(clang::CXXForRangeStmt* cxx_for_range_stmt) {
  set_cxx_for_range_stmt(cxx_for_range_stmt);
  set_is_loop(true);
  loop_info_.emplace(cxx_for_range_stmt);
  loop_info_->set_env_ptr(this);
}

void Env::update(clang::BinaryOperator* binary_operator) {
  std::string op = binary_operator->getOpcodeStr().str();

  binary_op_info_.emplace(op, binary_operator->getLHS(), binary_operator->getRHS());

  if (if_info_ && if_info_->if_stage() == IfStage::COND) {
    if_info_->update_check_equal(op);
  }

  if (op == "=") {
    if (binary_op_info_->left_expr_str() != binary_op_info_->right_expr_str()) {
      var_decls_[binary_op_info_->left_expr_str()] = binary_operator->getRHS();
    }
  }
}

void Env::update(clang::CXXOperatorCallExpr* cxx_operator_call_expr) {
  std::string op = stmt_to_string(cxx_operator_call_expr->getCallee());

  if (cxx_operator_call_expr->getNumArgs() == 2) {
    binary_op_info_.emplace(op, cxx_operator_call_expr->getArg(0), cxx_operator_call_expr->getArg(1));
  }

  // 在上一级声明, 在当前 Env 赋值。
  if (op == "operator=") {
    std::string var_name = stmt_to_string(cxx_operator_call_expr->getArg(0));
    if (parent_ != nullptr) {
      clang::Expr* value_expr = cxx_operator_call_expr->getArg(1);
      parent_->add(var_name, value_expr);

      std::vector<std::string> str_arr = absl::StrSplit(stmt_to_string(value_expr), ".");
      std::string callee_name = str_arr[0];
      if (find(callee_name) != nullptr && parent_ != nullptr) {
        LOG(INFO) << "add to parent, callee_name: " << callee_name
                  << ", expr: " << stmt_to_string(find(callee_name));
        parent_->add(callee_name, find(callee_name));
      }
    }
  }

  if (if_info_ && if_info_->if_stage() == IfStage::COND) {
    if_info_->update_check_equal(op);
  }
}

void Env::update(clang::CaseStmt* case_stmt) {
  switch_case_info_.emplace(case_stmt);
}

void Env::update_assign_info(clang::BinaryOperator* binary_operator) {
  std::string name = stmt_to_string(binary_operator->getLHS());
  assign_info_.emplace(name, binary_operator->getLHS(), binary_operator->getRHS());
}

absl::optional<std::string> Env::get_template_int_name(clang::Expr* init_expr) const {
  if (init_expr == nullptr) {
    return absl::nullopt;
  }

  if (clang::DeclRefExpr* decl_ref_expr = dyn_cast<clang::DeclRefExpr>(init_expr)) {
    if (decl_ref_expr->getDecl()->isTemplateParameter() &&
        decl_ref_expr->getType().getTypePtr()->isIntegerType()) {
      return absl::optional<std::string>(stmt_to_string(decl_ref_expr));
    }
  }

  return get_template_int_name(find(stmt_to_string(init_expr)));
}

bool Env::is_template_int_ref(clang::Expr* expr) const {
  absl::optional<std::string> int_name = get_template_int_name(expr);
  return int_name.has_value();
}

bool Env::is_reco_user_info() const {
  if (is_reco_user_info_) {
    return true;
  }

  if (parent_ != nullptr) {
    return parent_->is_reco_user_info();
  }

  return false;
}

void Env::set_is_reco_user_info(bool v) {
  if (parent_ == nullptr) {
    is_reco_user_info_ = v;
    return;
  }

  parent_->set_is_reco_user_info(v);
}

const std::string& Env::get_method_name() const {
  const Env* root = get_root();
  if (root != nullptr) {
    return root->method_name();
  }

  return method_name_;
}

bool Env::is_feature_other_method(const std::string& method_name) const {
  if (const auto& feature_info = get_feature_info()) {
    return feature_info->is_feature_other_method(method_name);
  }

  return false;
}

void Env::add_action_param_new_def(const std::string& prefix, const NewActionParam& new_action_param) {
  const auto new_params = new_action_param.new_params();
  for (size_t i = 0; i < new_params.size(); i++) {
    if (new_params[i].field() == "size") {
      add_new_def_meta(new_params[i].get_bs_enum_str(prefix),
                      new_params[i].get_new_def(prefix, this),
                      NewVarType::SCALAR);
    } else {
      add_new_def_meta(new_params[i].get_bs_enum_str(prefix),
                      new_params[i].get_new_def(prefix, this),
                      NewVarType::LIST);
    }
  }
}

const ConstructorInfo* Env::get_constructor_info() const {
  if (parent_ == nullptr) {
    return constructor_info_;
  }

  return parent_->get_constructor_info();
}

const FeatureInfo* Env::get_feature_info() const {
  if (parent_ == nullptr) {
    return feature_info_;
  }

  return parent_->get_feature_info();
}

ConstructorInfo* Env::mutable_constructor_info() {
  if (parent_ == nullptr) {
    return constructor_info_;
  }

  return parent_->mutable_constructor_info();
}

FeatureInfo* Env::mutable_feature_info() {
  if (parent_ == nullptr) {
    return feature_info_;
  }

  return parent_->mutable_feature_info();
}

std::vector<std::string> Env::find_new_action_param_var_name(const std::string& prefix,
                                                             const NewActionParam& new_action_param) const {
  std::vector<std::string> res;

  if (new_action_param.origin_name().size() > 0) {
    const auto& new_params = new_action_param.new_params();
    for (size_t i = 0; i < new_params.size(); i++) {
      std::string bs_enum_str = prefix + "_" + new_params[i].field();
      if (const auto& new_var = find_new_def(bs_enum_str)) {
        res.push_back(new_var->name());
      } else {
        LOG(INFO) << "cannot find action param new_var name in env, bs_enum_str: " << bs_enum_str
                  << ", prefix: " << prefix;
      }
    }
  }

  return res;
}

bool Env::is_in_loop_init() const {
  if (const auto& loop_info = cur_loop_info()) {
    if (loop_info->loop_stage() == LoopStage::INIT) {
      return true;
    }
  }

  return false;
}

bool Env::is_in_loop_body() const {
  if (const auto &loop_info = get_loop_info()) {
    if (loop_info->loop_stage() == LoopStage::BODY) {
      return true;
    }
  }

  return false;
}

bool Env::is_in_for_range_init() const {
  if (const auto &loop_info = cur_loop_info()) {
    if (loop_info->loop_stage() == LoopStage::INIT && !loop_info->is_for_stmt()) {
      return true;
    }
  }

  return false;
}

// 可能还有更多情况需要遍历。
bool Env::is_decl_ref_contains_self(clang::DeclRefExpr* decl_ref_expr,
                                    clang::Expr* value_expr) const {
  if (decl_ref_expr == nullptr || value_expr == nullptr) {
    return false;
  }

  if (stmt_to_string(decl_ref_expr) == stmt_to_string(value_expr)) {
    return true;
  }

  if (clang::ImplicitCastExpr* implicit_cast_expr = dyn_cast<clang::ImplicitCastExpr>(value_expr)) {
    if (is_decl_ref_contains_self(decl_ref_expr, implicit_cast_expr->getSubExpr())) {
      return true;
    }
  }

  if (clang::ParenExpr* paren_expr = dyn_cast<clang::ParenExpr>(value_expr)) {
    if (is_decl_ref_contains_self(decl_ref_expr, paren_expr->getSubExpr())) {
      return true;
    }
  }

  if (clang::BinaryOperator* binary_operator = dyn_cast<clang::BinaryOperator>(value_expr)) {
    if (is_decl_ref_contains_self(decl_ref_expr, binary_operator->getLHS())) {
      return true;
    }
    if (is_decl_ref_contains_self(decl_ref_expr, binary_operator->getRHS())) {
      return true;
    }
  }

  if (clang::CXXOperatorCallExpr* cxx_operator_call_expr = dyn_cast<clang::CXXOperatorCallExpr>(value_expr)) {
    for (size_t i = 0; i < cxx_operator_call_expr->getNumArgs(); i++) {
      if (is_decl_ref_contains_self(decl_ref_expr, cxx_operator_call_expr->getArg(i))) {
        return true;
      }
    }
  }

  return false;
}

bool Env::is_decl_in_parent_env(const std::string &var_name) const {
  if (parent_ != nullptr) {
    if (clang::DeclStmt* decl_stmt = parent_->get_decl_stmt_in_cur_env(var_name)) {
      return true;
    }
  }

  return false;
}

bool Env::is_decl_in_cur_env(const std::string &var_name) const {
  if (clang::DeclStmt* decl_stmt = get_decl_stmt_in_cur_env(var_name)) {
    return true;
  }

  return false;
}

bool Env::is_bslog_field_var_decl(const std::string& var_name) const {
  if (clang::DeclStmt *decl_stmt = get_decl_stmt(var_name)) {
    std::string decl_stmt_str = stmt_to_string(decl_stmt);

    if (decl_stmt_str.find("GetSingular") != std::string::npos ||
        decl_stmt_str.find("BSRepeatedField") != std::string::npos ||
        decl_stmt_str.find("BSMapField") != std::string::npos) {
      return true;
    }
  }

  return false;
}

std::unordered_map<std::string, BSFieldDetail> * Env::find_bs_field_detail_ptr_by_var_name(
  const std::string &var_name
) {
  if (bs_field_info_) {
    auto& map_field_detail = bs_field_info_->mutable_map_bs_field_detail();
    if (map_field_detail.find(var_name) != map_field_detail.end()) {
      return &map_field_detail;
    }
  }

  if (parent_ != nullptr) {
    return parent_->find_bs_field_detail_ptr_by_var_name(var_name);
  }

  return nullptr;
}

bool Env::is_in_parent_else() const {
  if (parent_ != nullptr && parent_->is_if()) {
    if (const auto& if_info = parent_->cur_if_info()) {
      if (if_info->if_stage() == IfStage::ELSE) {
        return true;
      }
    }
  }

  return false;
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
