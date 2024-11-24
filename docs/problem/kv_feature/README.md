# 迁移 `kv` 特征

如前文所描述，迁移 `kv` 特征主要是为了解决特征链路中存在的逻辑复用、血缘治理等问题。而关键的一个环节就是大量存量
平迁问题。

我们需要制定转换规则，将 `protobuf` 格式的数据转换为 `flatbuffers` 格式的数据，保证数据的一致性。并且还需要
将特征抽取类改写为读取 `kv` 数据的逻辑，保证逻辑对齐。

本章内容分为如下几个部分:

- [数据转换](problem/kv_feature/format_conversion.md)
- [特征改写](problem/kv_feature/feature_rewrite.md)
- [改写规则](problem/kv_feature/rewrite_rule/README.md)
  - [normal_adlog_field](problem/kv_feature/rewrite_rule/normal_adlog_field.md)
  - [common_info](problem/kv_feature/rewrite_rule/common_info.md)
  - [middle_node](problem/kv_feature/rewrite_rule/middle_node.md)
  - [action_detail](problem/kv_feature/rewrite_rule/action_detail.md)
  - [multi_action](problem/kv_feature/rewrite_rule/multi_action.md)
  - [reco_user_info](problem/kv_feature/rewrite_rule/reco_user_info.md)
  - [get_value_from_action](problem/kv_feature/rewrite_rule/get_value_from_action.md)
  - [photo_text](problem/kv_feature/rewrite_rule/photo_text.md)
  - [query_token](problem/kv_feature/rewrite_rule/query_token.md)
  - [other](problem/kv_feature/rewrite_rule/other.md)