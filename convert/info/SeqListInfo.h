#pragma once

#include <absl/types/optional.h>

#include <map>
#include <string>
#include <unordered_map>
#include "clang/AST/Expr.h"
#include "../Type.h"
#include "NewVarDef.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class SeqListInfo {
 public:
  explicit SeqListInfo(const std::string& root_name): root_name_(root_name) {}
  explicit SeqListInfo(const std::string& var_name,
                       const std::string& caller_name,
                       const std::string& type_str):
    var_name_(var_name),
    caller_name_(caller_name),
    type_str_(type_str) {}

  const std::string& root_name() const { return (root_name_); }
  const std::string& var_name() const { return (var_name_); }
  const std::string& caller_name() const { return (caller_name_); }
  const std::string& type_str() const { return (type_str_); }

  void update(const std::string &var_name,
              const std::string &caller_name,
              const std::string &type_str);

  std::string get_def() const;
  NewVarType get_var_type() const;

 private:
  std::string root_name_;
  std::string var_name_;
  std::string caller_name_;
  std::string type_str_;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
