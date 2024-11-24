#pragma once

#include <nlohmann/json.hpp>
#include <glog/logging.h>
#include <absl/types/optional.h>

#include <string>
#include "clang/AST/Expr.h"

#include "./RuleBase.h"
#include "../Type.h"
#include "../handler/StrictRewriter.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class Env;
using nlohmann::json;

/// 修复 BS 特征类中的变量声明顺序。
///
/// 尽量将 bs 字段获取逻辑放到使用前最近的地方，减少不必要的计算。
///
/// 示例: if 条件中不用的字段都应该放到 if body 中。
/// teams/ad/ad_algorithm/bs_feature/fast/impl/bs_extract_combine_reco_user_realtime_action_and_creative.cc
class BSFieldOrderRule: public RuleBase {
 public:
  using RuleBase::process;
  using RuleBase::process_to_json;

  explicit BSFieldOrderRule(clang::Rewriter& rewriter): RuleBase(rewriter, "BSFieldOrderRule") {}   // NOLINT

  /// 兼容接口，返回空 json。

  /// 删除位置不对的 decl_stmt, 并且通过 decl_stmt 修复 bs_field_enum 的定义位置。
  json process_to_json(clang::DeclStmt* decl_stmt, Env* env_ptr) override;

  /// 修复 bs 变量以及 bs_field_enum 变量的声明位置。
  ///
  /// bs 变量:
  /// 判断当前 bs 变量的 decl 是否在正确的 env 中，如果不对则挪到正确的 env 中。
  /// 如果是在 if 的 cond 或者 for 的 init 中，则其定义必须在父 env 中。
  /// 其他情况则必须在当前 env 中。
  ///
  /// bs_field_enum:
  /// bs_field_enum 的声明与使用肯定都是在 body 的同一 env 中。
  /// 如果当前 decl_ref 是 bs_field_enum, 并且在 decl_stmt 中，且 decl_stmt 包含
  /// GetSingular、BSRepeatedField 或者 BSMapField， 则可以将其声明与 var_name 绑定, 保存在
  /// env 中, 当之后遇到 var_name 时候则一起新增变量，同时将老的变量 decl 删掉。
  json process_to_json(clang::DeclRefExpr* decl_ref_expr, Env* env_ptr) override;

  /// 添加新增的变量定义。
  json process_to_json(clang::IfStmt *if_stmt, Env *env_ptr) override;

  /// 添加新增的变量定义。
  json process_to_json(clang::ForStmt *for_stmt, Env *env_ptr) override;

  /// 添加新增的变量定义。
  json process_to_json(clang::CXXForRangeStmt* cxx_for_range_stmt, Env* env_ptr) override;

  json process_to_json(clang::CXXMemberCallExpr* cxx_member_call_expr, Env* env_ptr) override;

  json process_to_json(clang::UnaryOperator *unary_operator, Env *env_ptr);

  void fix_bs_field_decl(const std::string& bs_var_name, Env* env_ptr, Env* target_env_ptr);

  void set_bs_field_visited(const std::string& bs_var_name, Env* env_ptr);

  bool is_has_value_in_bs_field_params(const std::string& bs_var_name, Env* env_ptr) const;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
