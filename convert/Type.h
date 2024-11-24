#pragma once

namespace ks {
namespace ad_algorithm {
namespace convert {

enum class ExprType {
  NONE,
  ADLOG_NORMAL,
  ADLOG_COMMON_INFO_SCALAR,
  ADLOG_COMMON_INFO_LIST,
  ADLOG_COMMON_INFO_MAP,
  ADLOG_MIDDLE_NODE_ROOT,

  /// attr.name_value()
  ADLOG_COMMON_INFO_NAME_VALUE,

  NULLPTR,
  TEMPLATE_INT_REF,
  INT,
  ACTION_DETAIL_FIXED_GET,
  ACTION_DETAIL_FIXED_HAS,
  REPEATED_PROTO
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
