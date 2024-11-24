#pragma once

#include <absl/types/optional.h>
#include <map>
#include <vector>
#include <string>
#include <unordered_map>
#include "clang/AST/Expr.h"
#include "../Type.h"
#include "./NewVarDef.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class Env;

struct BSFieldDetail {
  std::vector<std::string> enum_var_names;
  std::string new_def;
  NewVarType new_var_type;
  bool is_visited = false;
  bool is_has_value_in_params = false;
};

/// 保存当前 assign op 相关的信息。
class BSFieldInfo {
 private:
  /// bs field 变量到 bs_field_enum 变量的映射。
  /// 如 bs_field_enums_["user_id"] = { "enum_user_id"};
  std::unordered_map<std::string, BSFieldDetail> map_bs_field_detail_;

 public:
  BSFieldInfo() = default;

  const std::unordered_map<std::string, BSFieldDetail>& map_bs_field_detail() const {
    return map_bs_field_detail_;
  }

  std::unordered_map<std::string, BSFieldDetail>& mutable_map_bs_field_detail() {
    return map_bs_field_detail_;
  }

  /// 插入 bs_field_enum 与 bs_field va_name。
  void insert_bs_field_enum_var_names(const std::string &bs_var_name,
                                      const std::vector<std::string> &enum_var_names,
                                      bool is_has_value_in_params);

  const std::vector<std::string> & find_bs_field_enum_var_names(const std::string &bs_var_name) const;

  void insert_new_def(const std::string& var_name,
                      const std::string& new_def,
                      NewVarType new_var_type);
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
