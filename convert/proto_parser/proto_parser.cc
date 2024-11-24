#include <absl/strings/str_join.h>       // NOLINT
#include <absl/strings/str_split.h>
#include <glog/logging.h>
#include <google/protobuf/arena.h>
#include <google/protobuf/repeated_field.h>
#include <google/protobuf/descriptor.h>
#include <nlohmann/json.hpp>

#include <fstream>
#include <iomanip>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "./util.h"
#include "./proto_node.h"
#include "./proto_parser.h"
#include "proto/ad_joint_labeled_log.pb.h"

namespace ks {
namespace ad_algorithm {
namespace proto_parser {

using nlohmann::json;

using auto_cpp_rewriter::AdActionType;
using auto_cpp_rewriter::AdActionInfoList;
using auto_cpp_rewriter::AdJointLabeledLog;
using auto_cpp_rewriter::SimpleAdDspInfos;
using auto_cpp_rewriter::SimpleLiveInfos;

const AdlogNode* ProtoParser::adlog_root() const {
  if (adlog_root_ == nullptr) {
    return nullptr;
  }

  return adlog_root_.get();
}

std::unique_ptr<AdlogNode> ProtoParser::build_adlog_tree_using_reflection() {
  AdJointLabeledLog adlog;
  return build_adlog_tree_from_descriptor(adlog.GetDescriptor(), "adlog", 0, 0, "adlog", false);
}

void ProtoParser::add_single_enum(AdlogNode* root,
                                  const std::string& name,
                                  const EnumValueDescriptor* enum_value,
                                  const std::string& type_str) {
  if (root == nullptr || enum_value == nullptr) {
    return;
  }

  auto node = std::make_unique<AdlogNode>(enum_value->name(), type_str, enum_value->number());
  node->set_is_enum(true);
  root->add_child(name, std::move(node));
}

void ProtoParser::add_enum(AdlogNode* root,
                           const EnumDescriptor* enum_type,
                           const std::string& type_str,
                           const std::string& field_name) {
  if (root == nullptr || enum_type == nullptr) {
    return;
  }

  // 分别根据枚举名和 int 值建索引,
  std::string name = enum_type->name();
  for (int j = 0; j < enum_type->value_count(); j++) {
    const auto enum_value = enum_type->value(j);
    add_single_enum(root, enum_value->name(), enum_value, type_str);
    add_single_enum(root, std::to_string(enum_value->number()), enum_value, type_str);
  }

  if (field_name.size() > 0) {
    // 用于查找枚举字段
    auto node = std::make_unique<AdlogNode>(field_name, "int64", 0);
    node->set_is_enum(true);
    root->add_child(field_name, std::move(node));
  }
}

std::unique_ptr<AdlogNode> ProtoParser::build_adlog_tree_from_descriptor(const Descriptor* descriptor,
                                                                         const std::string& name,
                                                                         int index,
                                                                         int degree,
                                                                         const std::string& prefix,
                                                                         bool is_repeated) {
  if (is_descriptor_common_info(descriptor)) {
    return build_adlog_tree_common_info(descriptor, name, index, degree, prefix);
  }

  std::string type_name = descriptor->name();
  if (is_repeated) {
    type_name = std::string("repeated ") + type_name;
  }
  auto res = std::make_unique<AdlogNode>(name, type_name, index);

  for (int i = 0; i < descriptor->field_count(); i++) {
    const auto field = descriptor->field(i);

    if (field->name() == "serialized_reco_user_info") {
      // 不需要映射，直接获取原数据使用。
      auto node = std::make_unique<AdlogNode>(field->name(), field->type_name(), field->index());
      res->add_child(field->name(), std::move(node));
    } else if (const EnumDescriptor* enum_type = field->enum_type()) {
      add_enum(res.get(), enum_type, field->type_name(), field->name());
    } else if (field->is_map()) {
      // 必须在 message 判断之前, map 也是 message
      const auto key_field = field->message_type()->FindFieldByName("key");
      const auto value_field = field->message_type()->FindFieldByName("value");

      // value 是普通字段，直接当做叶子节点。
      if (is_basic_type(value_field->type())) {
        std::ostringstream oss_type_name;
        oss_type_name << "map<" << key_field->type_name() << ", " << value_field->type_name() << ">";
        auto node = std::make_unique<AdlogNode>(field->name(), oss_type_name.str(), field->index());

        res->add_child(field->name(), std::move(node));
      } else {
        // 中间的 map 按 key 的值展开，因此不需要关心 key，只需要按 value 继续展开。
        // 如 ActionDetail 类型是 map<int, SimpleAdDspInfos>, field->name 为 key。
        // 但是在获取 adlog_path 时候必须传入 key 的值。
        auto node = std::move(build_adlog_tree_from_descriptor(value_field->message_type(),
                                                               field->name(),
                                                               field->index(),
                                                               degree + 1,
                                                               prefix + "." + field->name(),
                                                               is_repeated));
        res->add_child(field->name(), std::move(node));
      }
    } else if (field->is_repeated()) {
      if (is_basic_type(field->type())) {
        auto node = std::make_unique<AdlogNode>(field->name(),
                                                std::string("repeated ") + field->type_name(),
                                                field->index());
        res->add_child(field->name(), std::move(node));
      } else {
        auto node = std::move(build_adlog_tree_from_descriptor(field->message_type(),
                                                               field->name(),
                                                               field->index(),
                                                               degree + 1,
                                                               prefix + "." + field->name(),
                                                               true));
        res->add_child(field->name(), std::move(node));
      }
    } else if (field->type() == FieldDescriptor::TYPE_MESSAGE) {
        auto node = std::move(build_adlog_tree_from_descriptor(field->message_type(),
                                                               field->name(),
                                                               i,
                                                               degree + 1,
                                                               prefix + "." + field->name(),
                                                               is_repeated));
      res->add_child(field->name(), std::move(node));
    } else if (is_basic_type(field->type())) {
      auto node = std::make_unique<AdlogNode>(field->name(), field->type_name(), field->index());
      res->add_child(field->name(), std::move(node));
    } else {
      LOG(INFO) << "ignore, field->type(): " << field->type_name()
                << ", field_name: " << field->name();
    }
  }

  if (res->is_action_detail_map()) {
    res->add_all_action_detail_field_types();
  }

  return res;
}

bool ProtoParser::is_descriptor_common_info(const Descriptor* descriptor) {
  if (descriptor == nullptr) {
    return false;
  }

  bool has_name_value = false;
  bool has_int_value = false;

  for (size_t i = 0; i < descriptor->field_count(); i++) {
    const auto field = descriptor->field(i);
    if (field->name() == "name_value") {
      has_name_value = true;
    } else if (field->name() == "int_value") {
      has_int_value = true;
    }
  }

  return has_name_value && has_int_value;
}

std::unique_ptr<AdlogNode> ProtoParser::build_adlog_tree_common_info(
  const Descriptor* descriptor,
  const std::string& name,
  int index,
  int degree,
  const std::string& prefix) {
  if (descriptor == nullptr) {
    return nullptr;
  }

  auto res = std::make_unique<AdlogNode>(name, descriptor->name(), index);
  if (descriptor->name() == "LabelAttr") {
    res->set_is_common_info_map(true);
  } else {
    res->set_is_common_info_list(true);
  }

  for (size_t i = 0; i < descriptor->enum_type_count(); i++) {
    const auto t = descriptor->enum_type(i);
    if (is_str_starts_with(t->name(), "Name")) {
      add_enum(res.get(), t, t->name(), name);
    }
  }

  return res;
}

bool ProtoParser::is_basic_type(FieldDescriptor::Type type) {
  if (type == FieldDescriptor::TYPE_MESSAGE || type == FieldDescriptor::TYPE_GROUP) {
    return false;
  }

  return true;
}

bool ProtoParser::is_common_info(const Descriptor* descriptor) {
  if (descriptor->FindEnumTypeByName("Name") != nullptr &&
      descriptor->FindFieldByName("name_value") != nullptr &&
      descriptor->FindFieldByName("int_list_value") != nullptr) {
    return true;
  }

  return false;
}

ProtoParser::ProtoParser() {
  LOG(INFO) << "start build adlog tree";
  adlog_root_ = std::move(build_adlog_tree_using_reflection());
  if (adlog_root_ == nullptr) {
    LOG(ERROR) << "build adlog tree failed!";
  }

  LOG(INFO) << "build adlog tree success!";
}

}  // namespace proto_parser
}  // namespace ad_algorithm
}  // namespace ks
