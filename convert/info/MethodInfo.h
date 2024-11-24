#pragma once

#include <absl/types/optional.h>

#include <vector>
#include <string>
#include <unordered_map>

#include "clang/AST/Type.h"

#include "NewActionParam.h"
#include "CommonInfoPrepare.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class MethodInfo {
 public:
  MethodInfo() = default;
  explicit MethodInfo(const std::string& name): name_(name) {}
  explicit MethodInfo(const std::string& name,
                      clang::QualType return_type,
                      const std::string& bs_return_type,
                      const std::string& decl,
                      const std::string& body):
    name_(name),
    return_type_(return_type),
    bs_return_type_(bs_return_type),
    decl_(decl),
    body_(body) {}

  const std::string& name() const { return name_; }
  clang::QualType return_type() const { return return_type_; }
  const std::string& bs_return_type() const { return bs_return_type_; }
  const std::string& decl() const { return decl_; }
  const std::string& body() const { return body_; }

  void update(clang::QualType return_type,
              const std::string& bs_return_type,
              const std::string& decl,
              const std::string& body);

  void add_new_action_param(size_t index,
                            const NewActionParam& new_action_param,
                            const std::string& new_origin_name);

  void add_new_action_param(size_t index,
                            const std::string& origin_name);
  void add_new_action_field_param(const std::string& origin_name,
                                  const std::string& field,
                                  const std::string& inner_type_str,
                                  bool is_combine_feature,
                                  const std::string& new_name);
  const NewActionParam& find_new_action_param(size_t index) const;

  absl::optional<std::vector<std::string>> find_new_action_param_name(size_t index) const;

  bool is_return_adlog_user_field() const { return is_return_adlog_user_field_; }
  void set_is_return_adlog_user_field(bool v) { is_return_adlog_user_field_ = v; }

  absl::optional<CommonInfoPrepare> &mutable_common_info_prepare() { return (common_info_prepare_); }
  const absl::optional<CommonInfoPrepare> &common_info_prepare() const { return (common_info_prepare_); }

 private:
  std::string name_;
  clang::QualType return_type_;
  std::string bs_return_type_;
  std::string decl_;
  std::string body_;

  /// AdActionInfo 对应到多个用到的字段，需要保存多个新参数。
  std::vector<NewActionParam> new_action_params_;

  bool is_return_adlog_user_field_ = false;
  absl::optional<CommonInfoPrepare> common_info_prepare_;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
