#pragma once

#include <google/protobuf/arena.h>
#include <google/protobuf/repeated_field.h>
#include <google/protobuf/descriptor.h>

#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "./proto_node.h"

namespace ks {
namespace ad_algorithm {
namespace proto_parser {

using ::google::protobuf::Descriptor;
using ::google::protobuf::EnumDescriptor;
using ::google::protobuf::FieldDescriptor;
using ::google::protobuf::EnumValueDescriptor;
using nlohmann::json;

/// 根据 proto 反射建立树节点
class ProtoParser {
  ProtoParser();
  std::unique_ptr<AdlogNode> adlog_root_ = nullptr;

  /// 如果 use_name_value_ = true, 则当通过 common info int value 查找 AdlogNode 时，如果找不到，
  /// 则新建一个 adlogNode，包含 int value 为值的枚举。
  bool use_name_value_ = false;

 public:
  static ProtoParser& instance() {
    static ProtoParser proto_parser;
    return (proto_parser);
  }

  /// 每个 message 对应一个节点，children_ 的 key 是 message 中 field 的字段名。嵌套类型再进行递归调用。
  /// 如果是 list 或者 map 的简单类型，则不需要递归，将当前字段作为叶子节点。
  ///
  /// field 的情况根据类型分以下几种:
  /// 1. 简单单值字段，如 int, float 等, name 为 children_ 的 key, ProtoField 为 value。
  /// 2. 简单 list 类型，list 中的数据为简单类型，不需要递归，将当前字段当做叶子节点的 ProtoField。
  /// 2. 嵌套 list 类型，list 中的数据为嵌套类型，需要递归，将当前 field 名作为 children_ 的 key, 嵌套
  ///    类型作为 value。
  /// 3. 简单 map 类型，不需要递归，将当前字段作为叶子节点的 ProtoField。
  /// 4. 嵌套 map 类型，需要递归，将当前 field 名作为 children_ 的 key, 嵌套的 value 类型作为 value，如
  ///    adlog.user_info.ad_dsp_action_detail。
  ///
  /// 注意 1: 在 message 中定义的内部 message 不会出现在 AdlogNode 中，只有 message 的字段才会成为 AdlogNode。
  /// 注意 2: LabelAttr 也是 CommonInfo 类型，但是 Name 枚举中只有 UNKNOW_NAME，查找 AdlogNode 时需要用
  ///        0 查询。
  /// 每个叶子节点对应一个 AdlogNode，包括简单类型、枚举、简单 list、简单 map。
  /// 简单类型以其变量名为 name, 枚举以其枚举名为 name。
  /// deprecate
  /// 补全类型、注释等信息。
  const AdlogNode* adlog_root() const;

  bool use_name_value() const { return use_name_value_; }
  void set_use_name_value(bool v) { use_name_value_ = v; }

  void add_single_enum(AdlogNode* root,
                       const std::string& name,
                       const EnumValueDescriptor* enum_value,
                       const std::string& type_str);

  void add_enum(AdlogNode* root,
                const EnumDescriptor* enum_type,
                const std::string& type_str,
                const std::string& field_name);

  // 通过反射构建 adlog tree。
  std::unique_ptr<AdlogNode> build_adlog_tree_using_reflection();
  std::unique_ptr<AdlogNode> build_adlog_tree_from_descriptor(const Descriptor* descriptor,
                                                            const std::string& name,
                                                            int index,
                                                            int degree,
                                                            const std::string& prefix,
                                                            bool is_repeated);

  /// 根据其 field 是否包含 name_value 判断是否是 CommonInfo。
  bool is_descriptor_common_info(const Descriptor* descriptor);

  /// CommonInfo 逻辑比较特殊，需要单独处理。其格式如下
  /// message CommonInfoAttr  {
  ///   enum Name {
  ///     UNKNOW_NAME = 0;
  ///     LIVE_RECO_EMBEDDING_CTR_USER = 1;
  ///     APP_LIST = 2;
  ///   };
  ///
  ///   enum NameExtendOne {
  ///     ECOM_BATCH_USER_SEARCH_1D_CAT2_LIST = 11000; // 用户 1 天搜索二级类目 ID
  ///     ECOM_BATCH_USER_SEARCH_1D_CAT3_LIST = 11001; // 用户 1 天搜索三级类目 ID
  ///   };
  ///
  ///   enum NameExtendTwo {
  ///     AKG_INDUS_USER_AD_INTEREST_SEQ_PAY_SUBMIT = 420004;
  ///     AKG_INDUS_USER_AD_INTEREST_SEQ_ITEM_CLICK = 420005;
  ///   };
  ///
  ///   optional CommonTypeEnum.AttrType type = 1;
  ///   optional int64 name_value = 2;
  ///   optional int64 int_value = 3;
  ///   optional float float_value = 4;
  ///   optional bytes string_value = 5;
  ///   repeated int64 int_list_value = 6;
  ///   repeated float float_list_value = 7;
  ///   repeated bytes string_list_value = 8;
  ///   map<int64, int64> map_int64_int64_value = 9;
  ///   map<string, int64> map_string_int64_value = 10;
  ///   map<int64, float> map_int64_float_value = 11;
  ///   map<string, float> map_string_float_value = 12;
  /// };
  ///
  /// 将枚举类型和值分别见索引，丢弃 int_value 等字段，实际使用时的类型是用户指定的。
  std::unique_ptr<AdlogNode> build_adlog_tree_common_info(const Descriptor* descriptor,
                                                          const std::string& name,
                                                          int index,
                                                          int degree,
                                                          const std::string& prefix);

  bool is_basic_type(FieldDescriptor::Type type);

  bool is_common_info(const Descriptor* descriptor);
};

}  // namespace proto_parser
}  // namespace ad_algorithm
}  // namespace ks
