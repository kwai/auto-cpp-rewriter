#pragma once

#include <glog/logging.h>
#include <absl/types/optional.h>
#include <set>
#include <map>
#include <utility>
#include <string>
#include <unordered_set>

#include "clang/AST/Expr.h"

#include "../Type.h"
#include "PrefixPair.h"
#include "CommonInfoPrepare.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class Env;
class StrictRewriter;

/// CommonAttrInfo, 包括 common info 枚举值，以及其他复杂逻辑。
/// common info 在第一次出现时候创建, prefix 是第一次出现 common info 时候可以确定的, 因此也是创建
/// common info 必须的参数。其他参数都是 absl::optional, 后面遇到了再更新。
///
/// 分为以下几种情况。
///
/// 1. 普通 common info, 用 CommonInfoType::NORMAL 表示。
///   主要分为两种情况: 单个 common info enum 和多个 common info enum, 都可以当做多个 common info enum 处理。
///   common info 中重要的几个信息都是按以下顺序在不同的地方出现的:
///   - prefix: 一定是第一次循环或者取变量时候出现, 如
///            const auto & common_info_attrs = item.ad_dsp_info().live_info().common_info_attr();
///   - common_info_value: 一定是 if 判断条件中出现的, 且 if 条件另一个变量一定是 xxx.name_value(), 判断相等
///   的值是 clang::DeclRefExpr* 或者 int, clang::DeclRefExpr* 表示枚举, int 表示枚举对应的 int 值，可能是
///            模板参数传进来的。如
///            if (attr.name_value() ==
///                         auto_cpp_rewriter::CommonInfoAttr_NameExtendTwo_LSP_LATEST_LIVE_SEGMENT_INFO_LIVE_ID) {
///            ...
///            }
///   - method_name: 一定是在 if body 中调用取值时候出现的, 取值类型可以根据 method_name 判断出来, 如
///            attr.int_value(), attr.int_list_value()
///
///   需要注意的是, 枚举可能只出现在模板参数里, 类的处理逻辑里并不会出现, 但是 bs 的 common info enum 必须在
///   解析时候就知道, 才能生成对应的 bs 代码, 参考:
///   teams/ad/ad_algorithm/feature/fast/impl/extract_live_lsp_segment_info.h
///   这种情况需要在解析前将当前文件中所有的枚举都找出来, 然后传给 FeatureInfo, 之后再替换时候再加上这部分。
///   因为 common info 枚举格式都比较固定, 因此可以直接用正则提取出来。
///
///
///  a. 单个 common info enum
///     auto attr = adlog.item.common_info_attr(i)
///     attr.int_value(), attr.int_list_value(), kv.first
///
///     单个 common info enum 的判断方式又分为两种情况, 一种是等于, 一种是不等于。不等于的情况需要在
///     CommonInfoNormal 中进行标记, 并且最后再替换的时候只需要将整个 loop 的 body 部分都替换即可。
///
///  b. 多个 common info, 在 for 循环中用 if 判断枚举来区分, 并且需要在 if env 中记录当前下标。
///   如 teams/ad/ad_algorithm/feature/fast/impl/extract_live_lsp_segment_info.h
///
///  const auto & common_info_attrs = item.ad_dsp_info().live_info().common_info_attr();
///  for (const auto & attr : common_info_attrs) {
///    if (attr.name_value() == attr_name) {
///      for (int64 value : attr.int_list_value()) {
///        if (variant == 0) {
///          AddFeature(value, 1.0f, result);
///          continue;
///        }
///        if (attr_name == auto_cpp_rewriter::CommonInfoAttr_NameExtendTwo_LSP_LATEST_LIVE_SEGMENT_INFO_LIVE_ID) {
///          if (variant == 1) {
///            AddFeature(value & MASK48, 1.0f, result);
///          } else if (variant == 2) {
///            AddFeature(value >> 48, 1.0f, result);
///          }
///          continue;
///        }
///       if (attr_name == auto_cpp_rewriter::CommonInfoAttr_NameExtendTwo_LSP_LATEST_LIVE_SEGMENT_INFO_TIMESTAMP) {
///         if (variant == 1) {
///           AddFeature((adlog.Get().time() /     1000 - value) /     60, 1.0f, result);
///         }
///         continue;
///       }
///      ...
///     }
///   }
///
///   需要判断每个枚举, 并将对应的逻辑转换为 bs 的逻辑。
///
///  枚举也可能是模板参数传进来，因此 if 条件比较的也可能是 int。int 的值在解析的时候取不到, 因此只能用变量的
///  方式保存。使用的时候用类似中间节点的方式。需要在 CommonInfoDetail 中进行区分。
///
/// 2. 多个 common info, 提前用 map 保存起来, 用 CommonInfoType::MULTI_MAP 表示。
/// 此种情况比较复杂, 提特征的逻辑见:
/// teams/ad/ad_algorithm/feature/fast/impl/extract_combine_realtime_action_match_cnt_v2.h 。
/// 构造函数里保存了一个 common info enum 到下标 int 的 map, 然后依次判断枚举对应的 common info 是否存在,
/// 如果存在则将对应的值保存到另一个数组对应的下标中。之后再遍历其中的 int_list value ，作为特征。
/// 先假设都是 int_list。后面再处理其他类型。
///
/// 会用到多个 common info enum, 如下
/// for (const ::auto_cpp_rewriter::CommonInfoAttr& user_attr : adlog.user_info().common_info_attr()) {
///   auto iter = user_attr_map_.find(user_attr.name_value());
///   if (iter != user_attr_map_.end()) {
///     int index = iter->second;
///     action_list[index] = user_attr;
///     cnt++;
///   }
/// }
/// 和普通的 common info 用 if (name_value == enum) 判断不同，这个用了一个 user_attr_map_ 提前存的 key
/// 来表示要用到的 common info enum。需要改成按 user_attr_map_ 来遍历。
/// for (auto it = user_attr_map_.begin(); it != user_attr_map_.end(); it++) {
///   BSReaptedField<int64_t> user_attr(*bs, it->first, pos);
///   int index = iter->second;
///   action_list[index] = user_attr;
///   cnt++;
/// }
///
/// 当遇到 user_attr_map_.find(user_attr.name_value()), 就可以确定是 MULTI_MAP 类型, 可以确定其 attr_map 以及
/// user_attr。
///
/// 4. 多个 common info, 提前用 map 保存起来，但是类型都是 int64_list, 用 CommonInfoType::MULTI_INT_LIST 表示。
/// 示例: teams/ad/ad_algorithm/feature/fast/impl/extract_user_rtl_product_name_shallow_action_7d.h
/// 详细逻辑见 docs/common_info.md。
///
/// 34 中间节点的 common info
///  示例: teams/ad/ad_algorithm/feature/fast/impl/extract_item_goods_id_list_size.h, 所需字段来自中间
///   节点的 common info, 如 live info common info。
///
/// 确定类型之前的信息都保存在 CommonInfoPrepare 中, 一旦确定类型, 设置 is_confirmed 为 true。
enum class CommonInfoType {
  NORMAL,
  MULTI_MAP,
  FIXED,
  MIDDLE_NODE,
  MULTI_INT_LIST
};

enum class CommonInfoValueType {
  BOOL,
  INT,
  FLOAT,
  STRING,
  BOOL_LIST,
  INT_LIST,
  FLOAT_LIST,
  STRING_LIST,
  MAP_INT_BOOL,
  MAP_INT_INT,
  MAP_INT_FLOAT,
  MAP_INT_STRING,
  MAP_STRING_INT,
  MAP_INT_MULTI_INT
};

class CommonAttrInfo {
 public:
  CommonAttrInfo() = default;
  explicit CommonAttrInfo(CommonInfoType common_info_type): common_info_type_(common_info_type) {}

  void set_env_ptr(Env* env_ptr);
  Env* env_ptr() const;
  Env* parent_env_ptr() const;

  bool is_normal() const { return common_info_type_ == CommonInfoType::NORMAL; }
  bool is_middle_node() const { return common_info_type_ == CommonInfoType::MIDDLE_NODE; }

  const absl::optional<CommonInfoPrepare>& get_common_info_prepare() const;

  /// common info method 相关
  static const std::string bool_value;
  static const std::string int_value;
  static const std::string float_value;
  static const std::string string_value;
  static const std::string bool_list_value;
  static const std::string int_list_value;
  static const std::string float_list_value;
  static const std::string string_list_value;
  static const std::string map_unit64_bool_value;
  static const std::string map_int64_int64_value;
  static const std::string map_int64_float_value;
  static const std::string map_int64_string_value;
  static const std::string map_string_int64_value;
  static const std::string map_int64_multi_value;
  static const std::string common_info_attr_size;

  static const std::unordered_set<std::string> scalar_method_names;
  static const std::unordered_set<std::string> list_method_names;
  static const std::unordered_set<std::string> map_method_names;
  static const std::unordered_set<std::string> repeated_size_method_names;

  static bool is_common_info_scalar_method(const std::string& method_name);
  static bool is_common_info_list_method(const std::string& method_name);
  static bool is_common_info_map_method(const std::string& method_name);
  static bool is_common_info_method(const std::string& method_name);
  static bool is_common_info_size_method(const std::string& method_name);
  static bool is_common_info_list_size_method(const std::string& method_name);
  static bool is_common_info_map_size_method(const std::string& method_name);
  static bool is_common_info_leaf_method(const std::string& method_name);

  static bool is_repeated_common_info_size(const std::string& method_name);

  static absl::optional<CommonInfoValueType> find_value_type(const std::string& method_name);

  /// scalar 或者 list 取的是内部数据的类型, map 是 value 的类型。
  static std::string get_inner_type_str(CommonInfoValueType value_type);

  static std::pair<std::string, std::string> get_map_inner_type_str(CommonInfoValueType value_type);
  static std::string get_bs_type_str(const std::string& method_name, bool is_combine_user);

  static bool is_common_info_list_or_map_loop(const std::string& s);
  static bool is_common_info_list_or_map_loop(const std::string& s, const std::string& loop_name);

  static bool contains_size_method(const std::string& s);

 protected:
  Env* env_ptr_ = nullptr;
  CommonInfoType common_info_type_;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
