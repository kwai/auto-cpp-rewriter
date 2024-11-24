#include <string>
#include <regex>
#include <algorithm>

#include <glog/logging.h>
#include <gflags/gflags.h>
#include <absl/strings/str_split.h>
#include <absl/strings/str_join.h>

#include "NewVarDef.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

void NewVarDef::set_name(const std::string& name) {
  if (name != name_) {
    std::regex p_name(std::string(" ") + name_ + " ");
    if (var_def_.size() > 0) {
      var_def_ = std::regex_replace(var_def_, p_name, std::string( " ") + name + std::string(" "));
    }

    name_ = name;
  }
}

void NewVarDef::set_exists_var_def(const std::string& exists_name, const std::string& exists_var_def) {
  exists_name_ = exists_name;
  exists_var_def_ = exists_var_def;
}

void NewVarDef::set_var_def(const std::string& var_def, NewVarType new_var_type) {
  var_def_ = var_def;
  new_var_type_ = new_var_type;
}

bool NewVarDef::is_list() const {
  if (new_var_type_) {
    if (*new_var_type_ == NewVarType::LIST) {
      return true;
    }
  }

  return false;
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
