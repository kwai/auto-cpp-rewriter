#pragma once

#include <nlohmann/json.hpp>
#include <absl/types/optional.h>
#include <string>
#include <memory>
#include <utility>
#include <vector>
#include <unordered_set>
#include <unordered_map>

namespace ks {
namespace ad_algorithm {
namespace proto_parser {

using nlohmann::json;

enum class AdlogPathType {
  NONE,
  /// 普通字段，如 adlog.user_info.id
  NORMAL,

  /// CommonInfo，如 adlog.user_info.common_info_attr.APP_LIST:int64_list
  COMMON_INFO,

  /// ActionDetailMap, 如 adlog.user_info.ad_dsp_action_detail.key:1.list
  ///
  /// 或者 UserActionDetail, 如 adlog.user_info.action_detail.follow.id
  ///
  /// message UserActionDetail {
  ///     repeated SimpleUserInfo follow = 1;
  ///     repeated SimplePhotoInfo like = 2;
  ///     repeated SimplePhotoInfo forward = 3;
  /// }
  ///
  /// message SimpleUserInfo {
  ///     uint64 id = 1;
  ///     uint64 action_time = 2;
  ///     uint64 photo_id = 3;
  /// };
  ACTION_DETAIL_LIST,

  /// ActionDetail, 如 adlog.user_info.ad_dsp_action_detail.key:1.list.photo_id
  ACTION_DETAIL_LEAF,

  /// LabelInfos map， 如 adlog.item.label_info.label_infos
  LABEL_INFO_MAP,

  /// LabelInfos 叶子节点， 如 adlog.item.label_info.label_infos.key:189
  LABEL_INFO_LEAF,

  /// repeated DeviceInfo leaf, 如 adlog.user_info.ad_user_info.device_info.os_type
  REPEATED_DEVICE_INFO_LEAF,

  /// crowd tag leaf
  CROWD_TAG_LEAF,

  ENUM
};

class AdlogNode;

struct AdlogPathDetail {
  AdlogPathDetail() = default;

  explicit AdlogPathDetail(const AdlogNode* adlog_node_ptr) :
    adlog_node(adlog_node_ptr), adlog_path_type(AdlogPathType::NORMAL) {}
  explicit AdlogPathDetail(const AdlogNode* adlog_node_ptr, AdlogPathType adlog_path_type):
    adlog_node(adlog_node_ptr), adlog_path_type(adlog_path_type) {}

  const AdlogNode* adlog_node = nullptr;

  AdlogPathType adlog_path_type = AdlogPathType::NONE;

  std::string type_str;

  absl::optional<int> action;
  absl::optional<int> label_info_value;

  std::shared_ptr<AdlogNode> enum_node = nullptr;

  void add_common_info_node(const AdlogNode* parent, int64_t int_value);
};

/// adlog 对应的 proto 节点，包含其中的字段信息。children_ 表示 message 中的各个字段。
/// 每个节点都必须有名字和类型。如果是叶子节点, 则 children_ 为空。
class AdlogNode {
  std::string name_;

  std::string type_str_;

  int index_;

  bool is_enum_ = false;

  /// 比较特殊，单独标记。
  bool is_common_info_list_ = false;
  bool is_common_info_map_ = false;

  std::string comment_;

  std::unordered_map<std::string, std::unique_ptr<AdlogNode>> children_;

  const AdlogNode* parent_ = nullptr;

  std::unordered_map<std::string, std::string> action_detail_field_types_;

 public:
  AdlogNode() = default;
  explicit AdlogNode(const std::string& name, const std::string& type_str, int index);

  const std::string& name() const { return name_; }

  std::string type_str() const;

  const std::string& comment() const { return comment_; }

  int index() const { return index_; }

  void set_type_str(const std::string& type_str) { type_str_ = type_str; }

  bool is_repeated() const { return type_str_.find("repeated") != std::string::npos; }

  const AdlogNode* parent() const;
  void set_parent(AdlogNode* parent);
  void set_parent(const AdlogNode* parent);

  void set_is_enum(bool is_enum) { is_enum_ = is_enum; }
  bool is_enum() const { return is_enum_; }

  std::string get_adlog_path() const;

  /// 注意 1: ActionDetail 和 label_infos 需要 map 的 key，是其他地方的变量，因此调用放需要传入参数 key。
  /// 注意 2: 可能会有来自 label_infos 的 CommonInfo, 逻辑比较特殊，和普通的 CommonInfo 有区别。
  ///        如 adlog.item.label_info.label_infos.key:232, LabelAttr 中没有枚举，只有保存值的几个字段，
  ///        因此解析 proto 时候并不能知道最终数据里的 key, 需要外面保存起来，在获取 adlog_path 时候
  ///        作为参数传进来。
  ///
  /// map<uint64, LabelAttr> label_infos = 160;   // 新 label 都放在这里，key 是 labelId
  ///
  /// message LabelAttr {
  ///   enum Name { UNKNOW_NAME = 0; };
  ///   optional LabelCommonTypeEnum.AttrType type = 1;
  ///   optional int64 name_value = 2;
  ///   optional int64 int_value = 3;
  ///   optional float float_value = 4;
  ///   optional bool bool_value = 5;
  ///   optional string string_value = 6;
  ///   map<int64, int64> map_int64_int64_value = 7;
  ///   map<int64, string> map_int64_string_value = 8;
  ///   optional uint64 first_occur_timestamp = 99;  // 该 label 首次出现时间点
  /// }
  std::string get_adlog_path(int key) const;
  std::string get_bslog_path() const;
  std::string get_bslog_path(int key) const;

  void set_is_common_info_list(bool is_common_info_list) { is_common_info_list_ = is_common_info_list; }

  /// label_infos map
  void set_is_common_info_map(bool is_common_info_map) { is_common_info_map_ = is_common_info_map; }

  /// 是否是 CommonInfo list 节点，如 adlog.user_info.common_info_attr
  bool is_common_info_list() const { return is_common_info_list_; }

  /// 是否是 CommonInfo map 节点，如 adlog.item.label_info.label_infos
  bool is_common_info_map() const { return is_common_info_map_; }

  /// 是否是 CommoInfo 叶子节点，如 adlog.user_info.common_info_attr.APP_LIST:int64_list
  bool is_common_info_leaf() const;

  /// 是否是 tube_item_info 字段，特殊处理，不以 adlog 开头。tube_item_info 只有 bs 数据，没有 adlog 数据。
  bool is_tube_item_info() const;

  /// 是否是 tube_user_info 字段，特殊处理，不以 adlog 开头。tube_user_info 只有 bs 数据，没有 adlog 数据。
  bool is_tube_user_info() const;

  /// 是否是 crowd_tag 字段，特殊处理，不以 adlog 开头。
  bool is_repeated_crowd_tag() const;

  /// 是否是 crowd_tag 字段，特殊处理，不以 adlog 开头。
  bool is_parent_repeated_crowd_tag() const;

  /// 示例: adlog.user_info.ad_user_info.device_info.app_package
  ///
  /// message DeviceInfo {
  ///   enum OsType {
  ///     UNKNOWN_OS_TYPE = 0;  // Unknown
  ///     ANDROID = 1;          // Android
  ///     IOS = 2;              // iOS
  ///   };
  ///   ...
  /// };
  bool is_device_info() const;
  bool is_repeated_device_info() const;

  bool is_device_info_leaf() const;
  bool is_repeated_device_info_leaf() const;

  /// 是否是 label_infos map 节点，adlog.item.label_info.label_infos。
  bool is_label_infos_map() const;

  /// 是否是 label_infos 叶子节点，如 adlog.item.label_info.label_infos.key:572。
  bool is_label_infos_leaf() const;

  /// 当前类型是否是 LabelAttr
  bool is_type_str_label_attr() const { return type_str_ == "LabelAttr"; }

  void add_child(const std::string& child_name, std::unique_ptr<AdlogNode> child) {
    child->set_parent(this);
    children_.insert({child_name, std::move(child)});
  }

  bool has_child(const std::string& child_name) const;

  /// 寻找 path 中的 key int, 如 key:2, key_2, key:2.key
  absl::optional<int> get_int_value_from_path(const std::string& bs_enum_str) const;

  /// bs_enum_str 是 adlog field 对应的 bs 枚举字符串
  std::string find_field_comment(const std::string& bs_enum_str) const;
  std::string find_field_type_str(const std::string& bs_enum_str) const;

  /// 可能会有多个同名的 proto message
  void find_middle_node_root(const std::string& middle_node_root,
                             std::vector<const AdlogNode*>* root_arr) const;

  bool is_str_all_uppercase(const std::string& s) const;

  /// 可能包含类型，需要去掉类型, 如 adlog.user_info.common_info_attr.APP_LIST:int64_list
  const AdlogNode* find_proto_node(const std::string& adlog_field_str) const;

  /// 查找 node
  const AdlogNode* find_proto_node_helper(const std::string& adlog_field_str) const;

  absl::optional<AdlogPathDetail> find_proto_path_detail(const std::string& adlog_field_str) const;
  absl::optional<AdlogPathDetail> find_proto_path_detail_helper(const std::string& adlog_field_str) const;

  /// 返回叶子节点完整的 CommonInfo 枚举名, 如 CommonInfoAttr::APP_LIST。
  /// field_path 是 adlog 路径，如 adlog.user_info.common_info_attr.APP_LIST:int64_list
  absl::optional<std::string> find_common_info_leaf_enum_name(const std::string& adlog_field_str) const;

  /// 是否是 ActionDetail map 节点，如 adlog.user_info.ad_dsp_action_detail
  bool is_action_detail_map() const;

  /// 是否是 ActionDetail 叶子节点，如 adlog.user_info.ad_dsp_action_detail.key:2.list.photo_id
  bool is_action_detail_leaf() const;

  /// ActionDetail map 的 value 或者 UserActionDetail 中的 SimpleUserInfo
  bool is_action_detail_list() const;
  bool is_user_action_detail() const;

  /// adlog.user_info.action_detail.follow.id
  bool is_from_user_action_detail_list() const;
  bool is_from_list() const;

  /// 有列表类型需要处理
  std::string get_type_str() const;

  /// CommonInfo 节点
  const AdlogNode* find_common_info_leaf_by_enum_value(int enum_value) const;
  const AdlogNode* find_common_info_leaf_by_enum_str(const std::string& enum_str) const;

  void add_all_action_detail_field_types();

  void insert_action_detail_field_type(const std::string& field_name,
                                       const std::string& type_str);

  const std::unordered_map<std::string, std::string>& action_detail_field_types() const {
    return action_detail_field_types_;
  }

  const std::unordered_map<std::string, std::unique_ptr<AdlogNode>>& children() const {
    return children_;
  }

  /// 主要用于中间节点。
  json to_json() const;

  bool is_field_non_proto(const std::string& adlog_field) const;
};

/// TODO(liuzhishan): 和其他地方逻辑有重复，之后单独拆一个 lib 公用。
class AdlogFieldDetail {
 public:
  AdlogFieldDetail() = default;

  explicit AdlogFieldDetail(const std::string& adlog_path) : adlog_path_(adlog_path) {}
  explicit AdlogFieldDetail(const std::string& adlog_path, int32_t attr_id, const std::string& type_str)
      : adlog_path_(adlog_path), attr_id_(attr_id), type_str_(type_str) {
    attr_id_str_ = std::to_string(attr_id_);
  }

  const std::string& adlog_path() const { return adlog_path_; }

  int32_t attr_id() const { return attr_id_; }

  const std::string& attr_id_str() const { return attr_id_str_; }

  const std::string& type_str() const { return type_str_; }

  void set_attr_id(int32_t attr_id) {
    attr_id_ = attr_id;
    attr_id_str_ = std::to_string(attr_id_);
  }
  void set_type_str(const std::string& type_str) { type_str_ = type_str; }

  bool is_valid() const { return adlog_path_.size() > 0; }

  bool is_int64() const;
  bool is_int64_list() const;
  bool is_float() const;
  bool is_float_list() const;
  bool is_str() const;
  bool is_str_list() const;

  bool is_user_field() const;
  bool is_context_field() const;
  bool is_item_field() const;

 private:
  std::string adlog_path_;
  int32_t attr_id_ = 0;
  std::string attr_id_str_;
  std::string type_str_;
};

}  // namespace proto_parser
}  // namespace ad_algorithm
}  // namespace ks
