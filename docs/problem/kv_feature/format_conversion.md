# 数据转换

## 目标格式

由于 `protobuf` 定义的数据格式太复杂，并且序列化与反序列开销也比较大，因此我们打算将数据格式迁移为 `flatbuffers`
格式的 `kv` 数据。

`flatbuffers` 相关知识可参考其官网: https://flatbuffers.dev/。

与 `protobuf` 相比，其优点是读取性能比较好，直接采用指针读取数据，几乎不需要反序列化时间。但缺点是数据是只读的，
使用起来不是很方便。

## 如何将 `AdjointLabeledLog` 转换为 `flatbuffers` 格式 ?

我们需要考虑如何将 `AdjointLabeledLog` 转换为 `flatbuffers` 格式, 并保证数据的唯一映射。

### `flatbuffers` 数据格式定义

```flatbuffers
namespace ks.cofea_fbs;

table BytesList {
  value:[string];
}

table FloatList {
  value:[float];
}

table Int64List {
  value:[long];
}

table RawFeature {
  kind:ks.cofea_fbs.RawFeature_.Anonymous0;
  name:string;
  is_list:bool;
}

namespace ks.cofea_fbs.RawFeature_;

table Anonymous0 {
  bytes_list:ks.cofea_fbs.BytesList;
  float_list:ks.cofea_fbs.FloatList;
  int64_list:ks.cofea_fbs.Int64List;
  simple_kscore_list:ks.cofea_fbs.SimpleKeyScoreList;
  bytes_msg_list:ks.cofea_fbs.BytesMsgList;
}

...

table FeatureSet {
  feature_name:string;
  raw_features:[ks.cofea_fbs.RawFeature];
}

table FeatureSets {
  feature_columns:[ks.cofea_fbs.FeatureSet];
}

table BatchedSamples {
  primary_key:ulong;
  timestamp:ulong;
  context:ks.cofea_fbs.FeatureSets;
  sub_context:[ks.cofea_fbs.FeatureSets];
  samples:ks.cofea_fbs.FeatureSets;
}
```

### 格式转换

我们可以观察到 `AdjointLabeledLog` 的结构中的路径是唯一的，因此我们可以用此路径作为唯一的 `key`，而 `value` 则可以
统一对应到几种基础类型。对于叶子节点为基础类型的简单嵌套字段，可以直接将路径作为`key`, 叶子节点的数据作为 `value`。

![pb_to_kv_convert.png](../images/problem/kv_feature/pb_to_kv_convert.png)