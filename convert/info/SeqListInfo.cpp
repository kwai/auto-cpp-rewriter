#include <glog/logging.h>
#include <sstream>

#include "../Tool.h"
#include "NewVarDef.h"
#include "SeqListInfo.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

void SeqListInfo::update(const std::string &var_name,
                         const std::string &caller_name,
                         const std::string &type_str) {
  var_name_ = var_name;
  caller_name_ = caller_name;
  type_str_ = type_str;
}

std::string SeqListInfo::get_def() const {
  std::ostringstream oss;
  oss << "const auto& " << var_name_ << " = " << root_name_;
  return oss.str();
}

NewVarType SeqListInfo::get_var_type() const {
  // ::google::protobuf::RepeatedField<::google::protobuf::int64>*
  if (type_str_.find("Repeated") != std::string::npos) {
    return NewVarType::LIST;
  } else {
    return NewVarType::SCALAR;
  }
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
