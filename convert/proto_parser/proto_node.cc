#include <glog/logging.h>               // NOLINT
#include <absl/strings/str_join.h>
#include <absl/strings/str_split.h>
#include <absl/strings/match.h>
#include <absl/types/optional.h>
#include <sstream>
#include <string>
#include <cctype>
#include <regex>
#include <unordered_set>
#include <unordered_map>

#include "./util.h"
#include "./proto_node.h"
#include "./proto_parser.h"

namespace ks {
namespace ad_algorithm {
namespace proto_parser {

void AdlogPathDetail::add_common_info_node(const AdlogNode* parent, int64_t int_value) {
  enum_node = std::make_shared<AdlogNode>(std::to_string(int_value), "int64", int_value);

  enum_node->set_parent(parent);
  enum_node->set_is_enum(true);

  adlog_path_type = AdlogPathType::COMMON_INFO;
  type_str = "int64";

  adlog_node = enum_node.get();
}

AdlogNode::AdlogNode(const std::string& name, const std::string& type_str, int index):
  name_(name), type_str_(type_str), index_(index) {}

const AdlogNode* AdlogNode::parent() const {
  return parent_;
}

void AdlogNode::set_parent(AdlogNode* parent) {
  parent_ = parent;
}

void AdlogNode::set_parent(const AdlogNode* parent) {
  parent_ = parent;
}

bool AdlogNode::has_child(const std::string& child_name) const {
  return children_.find(child_name) != children_.end();
}

std::string AdlogNode::get_adlog_path() const {
  if (parent_ == nullptr) {
    return name_;
  }

  if (is_common_info_leaf()) {
    return parent_->get_adlog_path() + std::string(".key:") + std::to_string(index_);
  } else if (is_action_detail_map()) {
    LOG(ERROR) << "cur node is action_detail_map, must call with action number: get_adlog_path(action)";
    return "";
  } else if (is_label_infos_leaf()) {
    LOG(ERROR) << "cur node is LabelAttr, must call with int key and label_info_key: get_adlog_path(key)";
    return "";
  } else {
    return parent_->get_adlog_path() + std::string(".") + name_;
  }
}

std::string AdlogNode::get_adlog_path(int key) const {
  if (parent_ == nullptr) {
    return name_;
  }

  if (is_common_info_leaf()) {
    return parent_->get_adlog_path(key) + std::string(".key:") + std::to_string(index_);
  } else if (is_action_detail_map()) {
    return parent_->get_adlog_path(key) + std::string(".") +
      name_ + std::string(".key:") + std::to_string(key);
  } else if (is_label_infos_leaf()) {
    return parent_->get_adlog_path(key) + std::string(".key:") + std::to_string(key);
  } else {
    return parent_->get_adlog_path(key) + std::string(".") + name_;
  }
}

std::string AdlogNode::get_bslog_path() const {
  if (parent_ == nullptr) {
    return name_;
  }

  if (is_tube_item_info()) {
    return "item_tube_item_info";
  } else if (is_tube_user_info()) {
    return "user_info_tube_user_info";
  } else if (is_common_info_leaf()) {
    return parent_->get_bslog_path() + std::string("_key_") + std::to_string(index_);
  } else if (is_action_detail_map()) {
    LOG(ERROR) << "cur node is action_detail_map, must call with action number: get_bslog_path(action)";
    return "";
  } else if (is_label_infos_leaf()) {
    LOG(ERROR) << "cur node is LabelAttr, must call with int key and label_info_key: get_bslog_path(key)";
    return "";
  } else {
    return parent_->get_bslog_path() + std::string("_") + name_;
  }
}

std::string AdlogNode::get_bslog_path(int key) const {
  if (parent_ == nullptr) {
    return name_;
  }

  if (is_tube_item_info()) {
    return "item_tube_item_info";
  } else if (is_tube_user_info()) {
    return "user_info_tube_user_info";
  } else if (is_common_info_leaf()) {
    return parent_->get_bslog_path(key) + std::string("_key_") + std::to_string(index_);
  } else if (is_action_detail_map()) {
    return parent_->get_bslog_path(key) + std::string("_") +
      name_ + std::string("_key_") + std::to_string(key);
  } else if (is_label_infos_leaf()) {
    return parent_->get_bslog_path(key) + std::string("_key_") + std::to_string(key);
  } else {
    return parent_->get_bslog_path(key) + std::string("_") + name_;
  }
}

absl::optional<int> AdlogNode::get_int_value_from_path(const std::string& bs_enum_str) const {
  // 注意: 只能包含一组 key:int, 如果有多个会有问题。
  // 目前 CommonInfo, ActionDetail, LabelAttr 都是一组 key:int。
  std::regex p("^key[_:](\\d+)([_\\.]key|[_\\.]value)?");
  std::smatch m;
  if (std::regex_search(bs_enum_str, m, p)) {
    if (m.size() > 1) {
      if (is_str_integer(m[1])) {
        return absl::make_optional(std::stoi(m[1]));
      } else {
        return absl::nullopt;
      }
    }
  }

  return absl::nullopt;
}

std::string AdlogNode::find_field_comment(const std::string& bs_enum_str) const {
  const AdlogNode* field = find_proto_node(bs_enum_str);
  if (field != nullptr) {
    if (field->is_enum()) {
      return field->name() + "," + field->comment();
    } else {
      return field->comment();
    }
  }

  return "";
}

std::string AdlogNode::find_field_type_str(const std::string& bs_enum_str) const {
  const AdlogNode* field = find_proto_node(bs_enum_str);
  if (field != nullptr) {
    return field->type_str();
  }

  return "";
}

void AdlogNode::find_middle_node_root(const std::string& middle_node_root,
                                      std::vector<const AdlogNode*>* root_arr) const {
  if (root_arr == nullptr) {
    return;
  }

  for (auto it = children_.begin(); it != children_.end(); it++) {
    if (it->second != nullptr) {
      it->second->find_middle_node_root(middle_node_root, root_arr);
    }
  }
}

bool AdlogNode::is_str_all_uppercase(const std::string& s) const {
  if (s.size() == 0) {
    return false;
  }

  for (size_t i = 0; i < s.size(); i++) {
    if (std::isupper(s[i]) || s[i] == '_' || std::isdigit(s[i])) {
      continue;
    }

    return false;
  }

  return true;
}

// 需要判断是否以类型结尾。
// 包含 : 的只有两种情况，一种是 CommonInfo, 如
// adlog.user_info.common_info_attr.APP_LIST:int64_list, adlog.user_info.common_info_attr.key:2:int64_list,
// 另一种是中间的 ActionDetail map, 如 adlog.user_info.ad_dsp_action_detail.key:3.list.photo_id。
//
// 可以按如下逻辑进行处理:
// 1. 按冒号拆分 adlog_field_str。
// 2. 判断拆分后的最后一个 str 是否是类型，所有的类型可以提前列举出来。
// 3. 如果最后一个 str 是类型，则将之前的 str 作为 find_proto_node_helper 的参数，否则将 adlog_field_str
//    整体作为 find_proto_node_helper 的参数。
const AdlogNode* AdlogNode::find_proto_node(const std::string& adlog_field_str) const {
  std::vector<std::string> arr = absl::StrSplit(adlog_field_str, ":");
  if (arr.size() == 0) {
    return nullptr;
  }

  if (is_unified_type_str(arr[arr.size() - 1])) {
    return find_proto_node_helper(absl::StrJoin(arr.begin(), arr.end() - 1, ":"));
  } else {
    return find_proto_node_helper(adlog_field_str);
  }
}

const AdlogNode* AdlogNode::find_proto_node_helper(const std::string& adlog_field_str) const {
  auto adlog_path_detail = find_proto_path_detail_helper(adlog_field_str);
  if (adlog_path_detail) {
    return adlog_path_detail->adlog_node;
  }
  return nullptr;
}

absl::optional<AdlogPathDetail>
AdlogNode::find_proto_path_detail(const std::string& adlog_field_str) const {
  std::vector<std::string> arr = absl::StrSplit(adlog_field_str, ":");
  if (arr.size() == 0) {
    return absl::nullopt;
  }

  if (is_unified_type_str(arr.back())) {
    auto res = find_proto_path_detail_helper(absl::StrJoin(arr.begin(), arr.end() - 1, ":"));
    if (res.has_value()) {
      res->type_str = arr.back();
    }
    return res;
  } else {
    return find_proto_path_detail_helper(adlog_field_str);
  }
}

absl::optional<AdlogPathDetail>
AdlogNode::find_proto_path_detail_helper(const std::string& adlog_field_str) const {
  if (adlog_field_str.size() == 0) {
    return absl::nullopt;
  }

  if (adlog_field_str == "adlog.is_train") {
    return absl::nullopt;
  }

  if (adlog_field_str == name_) {
    return absl::make_optional<AdlogPathDetail>(this);
  }

  if (!is_str_starts_with(adlog_field_str, name_)) {
    LOG(ERROR) << "adlog_field_str should starts with: " << name_ << ", but is : " << adlog_field_str;
    return absl::nullopt;
  }

  std::string field_name = "";
  std::string suffix = "";

  if (name_.size() + 1 >= adlog_field_str.size()) {
    return absl::nullopt;
  }

  std::string child_field_str = adlog_field_str.substr(name_.size() + 1);

  size_t pos = child_field_str.find(".");
  if (pos != std::string::npos) {
    field_name = child_field_str.substr(0, pos);
    if (pos + 1 < child_field_str.size()) {
      suffix = child_field_str.substr(pos + 1);
    }
  } else {
    field_name = child_field_str;
    suffix = "";
  }

  auto it = children_.find(field_name);
  if (it != children_.end() && it->second != nullptr) {
    // 可能是 action_detail 或者 common_info
    if (it->second->is_common_info_list()) {
      if (is_str_starts_with(suffix, "key:")) {
        if (absl::optional<int> int_value = get_int_value_from_path(suffix)) {
          if (const AdlogNode* node = it->second->find_common_info_leaf_by_enum_value(*int_value)) {
            return absl::make_optional<AdlogPathDetail>(node, AdlogPathType::COMMON_INFO);
          } else {
            // 如果 use_name_value = false, 则返回 absl::nullopt。
            // 否则新建一个包含 *int_value 的节点。
            bool use_name_value = ProtoParser::instance().use_name_value();

            if (use_name_value) {
              absl::optional<AdlogPathDetail> res = absl::make_optional<AdlogPathDetail>();
              res->add_common_info_node(it->second.get(), *int_value);
              return res;
            } else {
              return absl::nullopt;
            }
          }
        }
      } else if (suffix.size() == 0) {
        // CommonInfo 列表
        if (const AdlogNode* node = it->second.get()) {
          return absl::make_optional<AdlogPathDetail>(node, AdlogPathType::COMMON_INFO);
        } else {
          return absl::nullopt;
        }
      } else if (suffix.size() > 0 && is_str_all_uppercase(suffix)) {
        // 枚举名
        if (const AdlogNode* node = it->second->find_common_info_leaf_by_enum_str(suffix)) {
          return absl::make_optional<AdlogPathDetail>(node, AdlogPathType::COMMON_INFO);
        } else {
          return absl::nullopt;
        }
      } else if (is_nonstd_common_info_enum(suffix)) {
        // 少量枚举名不规范，包含小写字母，单独处理。
        if (const AdlogNode* node = it->second->find_common_info_leaf_by_enum_str(suffix)) {
          return absl::make_optional<AdlogPathDetail>(node, AdlogPathType::COMMON_INFO);
        } else {
          return absl::nullopt;
        }
      } else {
        LOG(ERROR) << "wrong format, should be enum str or enum value, but is: " << suffix;
        return absl::nullopt;
      }
    } else if (it->second->is_action_detail_map()) {
      if (is_str_starts_with(suffix, "key:")) {
        // action_detail
        // 需要去掉 .key:xxx, ActionDetail node 不包含 key，只有 value
        if (absl::optional<int> int_value = get_int_value_from_path(suffix)) {
          std::regex p_action("key:\\d+\\.");
          child_field_str = std::regex_replace(child_field_str, p_action, "");
          if (auto res = it->second->find_proto_path_detail_helper(child_field_str)) {
            res->action.emplace(int_value.value());
            res->adlog_path_type = AdlogPathType::ACTION_DETAIL_LEAF;
            return res;
          } else {
            return absl::nullopt;
          }
        } else {
          LOG(ERROR) << "cannot find action in child_field_str: " << child_field_str;
          return absl::nullopt;
        }
      } else if (suffix.size() == 0) {
        if (const AdlogNode* node = it->second.get()) {
          return absl::make_optional<AdlogPathDetail>(node, AdlogPathType::ACTION_DETAIL_LIST);
        } else {
          LOG(ERROR) << "cannot find adlog node: " << adlog_field_str;
          return absl::nullopt;
        }
      } else {
        LOG(ERROR) << "wrong format for action, should be .key:xxx, but is: " << suffix;
        return absl::nullopt;
      }
    } else if (it->second->is_label_infos_map()) {
      if (absl::StartsWith(suffix, "key:")) {
        if (absl::optional<int> int_value = get_int_value_from_path(suffix)) {
          // 固定是 0: UNKNOW_NAME
          if (const AdlogNode* node = it->second->find_common_info_leaf_by_enum_value(0)) {
            auto res = absl::make_optional<AdlogPathDetail>(node, AdlogPathType::LABEL_INFO_LEAF);
            res->label_info_value.emplace(*int_value);
            return res;
          } else {
            LOG(ERROR) << "cannot find labe_info, key: 0, adlog_field_str: " << adlog_field_str;
            return absl::nullopt;
          }
        } else {
          LOG(ERROR) << "cannot find int_value from suffix: " << suffix
                     << ", adlog_field_str: " << adlog_field_str;
          return absl::nullopt;
        }
      } else if (suffix.size() == 0) {
        if (const AdlogNode* node = it->second.get()) {
          return absl::make_optional<AdlogPathDetail>(node, AdlogPathType::LABEL_INFO_MAP);
        } else {
          LOG(ERROR) << "cannot find label info node, adlog_field_str: " << adlog_field_str;
          return absl::nullopt;
        }
      } else {
        LOG(ERROR) << "wrong format for label_infos, should be .key:xxx, but is: " << suffix;
        return absl::nullopt;
      }
    } else if (it->second->is_enum()) {
      return absl::make_optional<AdlogPathDetail>(it->second.get(), AdlogPathType::ENUM);
    } else {
      return it->second->find_proto_path_detail_helper(child_field_str);
    }
  }

  if (!is_field_non_proto(adlog_field_str)) {
    LOG(ERROR) << "cannot find proto path detail, wrong format of adlog_field_str: " << adlog_field_str
               << ", field_name: " << field_name;
  }

  return absl::nullopt;
}

absl::optional<std::string> AdlogNode::find_common_info_leaf_enum_name(
  const std::string& adlog_field_str) const {
  const AdlogNode* node = find_proto_node(adlog_field_str);
  if (node != nullptr && node->parent() != nullptr) {
    return absl::make_optional(node->parent()->type_str() + std::string("::") + node->name());
  }

  LOG(INFO) << "cannot find common info leaf enum name, adlog_field_str: " << adlog_field_str;
  return absl::nullopt;
}

bool AdlogNode::is_common_info_leaf() const {
  if (is_enum_) {
    if (parent_ != nullptr && parent_->is_common_info_list()) {
      if (!is_label_infos_leaf() && !is_label_infos_map()) {
        return true;
      }
    }
  }

  return false;
}

bool AdlogNode::is_tube_item_info() const {
  if (name_ == "tube_item_info") {
    return true;
  } else {
    return false;
  }
}

bool AdlogNode::is_tube_user_info() const {
  if (name_ == "tube_user_info") {
    return true;
  } else {
    return false;
  }
}

bool AdlogNode::is_repeated_crowd_tag() const {
  if (absl::EndsWith(type_str_, "StrategyCrowdTag")) {
    return true;
  }

  return false;
}

bool AdlogNode::is_parent_repeated_crowd_tag() const {
  if (parent_ != nullptr && parent_->is_repeated_crowd_tag()) {
    return true;
  }

  return false;
}

bool AdlogNode::is_device_info() const {
  return type_str_.find("DeviceInfo") != std::string::npos;
}

bool AdlogNode::is_device_info_leaf() const {
  if (parent_ != nullptr) {
    return parent_->is_device_info();
  }

  return false;
}

bool AdlogNode::is_repeated_device_info() const {
  return is_repeated() && is_device_info();
}

bool AdlogNode::is_repeated_device_info_leaf() const {
  if (parent_ != nullptr) {
    return parent_->is_repeated_device_info();
  }

  return false;
}

bool AdlogNode::is_label_infos_map() const {
  if (parent_ != nullptr) {
    if (parent_->name() == "label_info") {
      return name_ == "label_infos" || name_ == "global_gmv_label_infos";
    }
  }

  return false;
}

bool AdlogNode::is_label_infos_leaf() const {
  if (parent_ != nullptr) {
    if (parent_->is_label_infos_map()) {
      return true;
    }
  }

  return false;
}

bool AdlogNode::is_action_detail_map() const {
  static std::unordered_set<std::string> action_types = {
  "SimpleAdDspInfos", "SimpleAdDspInfosV2",
  "SimpleFansTopInfos", "SimpleLiveInfos", "AdActionInfoList"};
  return action_types.find(type_str_) != action_types.end();
}

bool AdlogNode::is_action_detail_leaf() const {
  if (parent_ != nullptr && parent_->parent() != nullptr) {
    if (parent_->parent()->is_action_detail_map()) {
      return true;
    }

    if (parent_->parent()->is_user_action_detail()) {
      return true;
    }
  }

  return false;
}

bool AdlogNode::is_user_action_detail() const {
  return type_str_ == "UserActionDetail";
}

bool AdlogNode::is_action_detail_list() const {
  if (parent_ != nullptr) {
    if (parent_->is_action_detail_map()) {
      return true;
    }

    if (parent_->is_user_action_detail()) {
      return true;
    }
  }

  return false;
}

bool AdlogNode::is_from_user_action_detail_list() const {
  if (is_user_action_detail()) {
    return true;
  }

  if (parent_ != nullptr) {
    return parent_->is_from_user_action_detail_list();
  }

  return false;
}

// 枚举字段不会有 chidlren_，只会有嵌套定义的枚举
const AdlogNode* AdlogNode::find_common_info_leaf_by_enum_value(int enum_value) const {
  auto it_child = children_.find(std::to_string(enum_value));
  if (it_child != children_.end()) {
    return it_child->second.get();
  }

  return nullptr;
}

const AdlogNode* AdlogNode::find_common_info_leaf_by_enum_str(const std::string& enum_str) const {
  auto it_child = children_.find(enum_str);
  if (it_child != children_.end()) {
    return it_child->second.get();
  }

  return nullptr;
}

void AdlogNode::add_all_action_detail_field_types() {
  if (!is_action_detail_map()) {
    return;
  }

  auto it = children_.find("list");
  if (it == children_.end()) {
    return;
  }

  const auto& m = it->second->children();
  for (auto it_field = m.begin(); it_field != m.end(); it_field++) {
    if (it_field->second->name().size() > 0) {
      action_detail_field_types_[it_field->second->name()] = it_field->second->type_str();
    } else {
      LOG(INFO) << "field name is empty! type_str: " << it_field->second->type_str();
    }
  }
}

void AdlogNode::insert_action_detail_field_type(const std::string& field_name, const std::string& type_str) {
  action_detail_field_types_[field_name] = type_str;
}

bool AdlogNode::is_from_list() const {
  if (parent_ != nullptr) {
    return parent_->is_from_list();
  }

  return type_str_.find("repeated") != std::string::npos;
}

std::string AdlogNode::type_str() const {
  if (absl::EndsWith(type_str_, "_t")) {
    return type_str_.substr(0, type_str_.size() - 2);
  } else {
    return type_str_;
  }
}

json AdlogNode::to_json() const {
  json res = json::object();

  // 不用存枚举。
  if (is_enum_) {
    return res;
  }

  res["name"] = name_;
  res["type_str"] = type_str_;
  res["children"] = json::object();

  for (auto it = children_.begin(); it != children_.end(); it++) {
    res["children"][it->first] = it->second->to_json();
  }

  return res;
}

bool AdlogNode::is_field_non_proto(const std::string& adlog_field) const {
  // 暂时简单处理，更精确的处理是判断 starts_with
  static std::unordered_set<std::string> method_names = {
  "reco_user_info",
  "ad_user_history_photo_embedding",
  "is_train",
  "ad_user_ad_action_map",
  "h_maplist_append_req",
  "colossus_ad_live_item",
  "colossus_reco_live_v1_item",
  "colossus_reco_live_item",
  "colossus_ad_goods_item",
  "colossus_ad_goods_item_new",
  "colossus_ad_goods_item_new_pdn_v0",
  "colossus_reco_photo",
  "colossus_reco_photo_v3",
  "colossus_reco_photo_v3_pdn_v0",
  "colossus_ad_live_spu",
  "colossus_ad_live_cid3",
  "author_cluster_map",
  "live_cluster_map",
  "ad_live_global_gsu_result",
  "ad_live_ecomm_gsu_result",
  "ad_live_author_gsu_result",
  "ad_live_spu_gsu_result",
  "ad_live_cid3_gsu_result",
  "ad_live_author_cluster_gsu_result",
  "ad_live_remote_cluster_gsu_result",
  "reco_live_global_gsu_result",
  "reco_live_author_gsu_result",
  "reco_live_author_cluster_gsu_result",
  "reco_live_remote_cluster_gsu_result",
  "reco_live_v1_remote_cluster_gsu_result",
  "ad_live_colossus_idx_filtered_by_playtime",
  "ad_live_colossus_idx_filtered_by_label",
  "reco_live_colossus_idx_filtered_by_playtime",
  "ad_user_all_goods_action",
  "ad_user_history_photo_embedding",
  "ad_user_history_pdct_embedding",
  "ad_user_histactpdct_pdct",
  "ad_user_histactpdct_firstid",
  "ad_user_histactpdct_secondid",
  "ad_user_histactpdct_u2uemb",
  "ad_user_histact_weight",
  "ad_user_histact_type",
  "picasso_ad_goods_item",
  "ad_live_delivering_author",
  "living_author_set",
  "ad_live_offline_sample_set",
  };

  for (const auto& method_name : method_names) {
    if (adlog_field.find(method_name) != std::string::npos) {
      return true;
    }
  }

  return false;
}


bool AdlogFieldDetail::is_int64() const {
  static std::unordered_set<std::string> types = {"int32_t", "int", "int64", "uint64", "int64_t", "uint64_t"};
  return types.find(type_str_) != types.end();
}

bool AdlogFieldDetail::is_int64_list() const {
  static std::unordered_set<std::string> types = {"int_list", "int64_list"};
  return types.find(type_str_) != types.end();
}

bool AdlogFieldDetail::is_float() const {
  static std::unordered_set<std::string> types = {"float", "double"};
  return types.find(type_str_) != types.end();
}

bool AdlogFieldDetail::is_float_list() const {
  static std::unordered_set<std::string> types = {"float_list", "double_list"};
  return types.find(type_str_) != types.end();
}

bool AdlogFieldDetail::is_str() const {
  static std::unordered_set<std::string> types = {"str", "string", "std::string", "absl::string_view"};
  return types.find(type_str_) != types.end();
}

bool AdlogFieldDetail::is_str_list() const {
  static std::unordered_set<std::string> types = {"str_list", "string_list", "std::vector<std::string>",
                                                  "std::vector<absl::string_view>"};
  return types.find(type_str_) != types.end();
}

bool AdlogFieldDetail::is_user_field() const { return is_str_starts_with(adlog_path_, "adlog.user"); }

bool AdlogFieldDetail::is_context_field() const { return is_str_starts_with(adlog_path_, "adlog.context"); }

bool AdlogFieldDetail::is_item_field() const { return is_str_starts_with(adlog_path_, "adlog.item"); }

}  // namespace proto_parser
}  // namespace ad_algorithm
}  // namespace ks
