#pragma once

#include <absl/types/optional.h>
#include <map>
#include <vector>
#include <string>
#include <unordered_map>
#include "clang/AST/Expr.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class Env;

/// 中间节点，如 PhotoInfo、LiveInfo 等。
/// 包含前缀等信息。
class MiddleNodeInfo {
 public:
  MiddleNodeInfo() = default;
  explicit MiddleNodeInfo(const std::string& name): name_(name) {}

  const std::string& name() const { return name_; }

  std::string get_bs_exists_field_def(Env* env_ptr,
                                      const std::string& leaf,
                                      const std::string& field) const;
  std::string get_root_bs_exists_field_def(Env* env_ptr) const;

  std::string get_bs_scalar_field_def(Env* env_ptr,
                                      const std::string& leaf,
                                      const std::string& field,
                                      clang::QualType qual_type) const;

  std::string get_bs_list_field_def(Env *env_ptr,
                                    const std::string &leaf,
                                    const std::string &field,
                                    const std::string& inner_type) const;

  std::string get_list_loop_bs_wrapped_text(Env* env_ptr,
                                            const std::string& body,
                                            const std::string& bs_enum_str) const;

  std::string get_bs_list_def(Env* env_ptr,
                              const std::string& bs_enum_str,
                              const std::string& middle_node_leaf,
                              const std::string& type_str) const;

  std::string get_bs_str_scalar_def(Env* env_ptr,
                                    const std::string& bs_enum_str,
                                    const std::string& middle_node_leaf) const;

  static bool is_user_middle_node(const std::string& name);
  static const std::vector<std::string>& get_possible_adlog_prefix(const std::string& root);

 protected:
  std::string name_;
  static std::unordered_map<std::string, std::vector<std::string>> possible_adlog_prefix_;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
