# 原始数据格式与特征提取

原始数据是以 `protobuf` 格式存储的，通过 `c++` 实现特征提取的逻辑。如下图所示:

![proto_data_and_feature](../../images/problem/origin_format/proto_data_and_feature.png)

## 原始数据格式: `AdJointLabeledLog`

`AdJointLabeledLog` 是 `protobuf` 定义的嵌套结构，用于保存特征提取所需要的原始数据，如下所示:

```proto
message CommonInfoAttr {
    enum Name {
        UNKNOW_NAME = 0;

        LIVE_RECO_EMBEDDING_CTR_USER = 1;
        APP_LIST = 2;
        CLICK_LIST_FOLLOW = 3;
        CLICK_LIST_HOT = 4;
        CLICK_LIST_NEAR = 5;
        FORWARD_LIST = 6;
    }

    optional CommonTypeEnum.AttrType type = 1;
    optional int64 name_value = 2;
    optional int64 int_value = 3;
    optional float float_value = 4;
    optional bytes string_value = 5;
    repeated int64 int_list_value = 6;
    repeated float float_list_value = 7;
    repeated bytes string_list_value = 8;
    map<int64, int64> map_int64_int64_value = 9;
    map<string, int64> map_string_int64_value = 10;
    map<int64, float> map_int64_float_value = 11;
    map<string, float> map_string_float_value = 12;
}

message UserInfo {
    uint64 id = 1;
    UserActionDetail action_detail = 2;
    repeated CommonInfoAttr common_info_attr = 3;
}

/////// Item 表示一个投放单元 ///////////////
message Item {
    string reason = 1;
    ItemType type = 2;
    uint64 id = 3;
    repeated CommonInfoAttr common_info_attr = 4;
}
//////////// AdJointLabeledLog /////////////////
message AdJointLabeledLog {
    uint64 llsid = 1;
    uint32 deprecated_tab = 2;
    uint64 time = 3;
    uint32 page = 4;
    UserInfo user_info = 5;
    repeated Item item = 6;
}
```


其字段主要分为两种类型:
- 基础类型以及普通嵌套字段类型，如 `UserInfo`、`PhotoInfo` 等。
- `CommonInfoAttr` 类型，为了更灵活的在同一结构体中保存不同类型的数据，采用类似 `json` 的方式，预留不同类型的字段，
  在使用时候根据枚举名区分不同的数据。因此使用时必须遍历整个 `CommonInfoAttr` 并根据枚举来判断是否是需要的数据。

注意: 为了简化变量名，以后用 `AdLog` 特指 `AdJointLabeledLog`。

## 特征提取

特征提取逻辑通过 `c++` 实现。采用继承的方式提供统一的接口。输入统一为 `AdLog` 类型和 `item` 下标。

不同的特征提取类继承自基类 `FastFeature`，并实现 `Extract` 方法。

如下所示:

```c++
/// 为了节省内存，这里使用 union 压缩了内存占用
/// 用法：需要在访问的地方判断是否是 sparse or dense。
/// 如果是 sparse feature， 只能访问 sign 字段
/// 如果是 dense feature，只能访问 value 和 dense_sign 字段
union ExtractResult {
  /// for sparse feature only
  uint64_t sign;

  /// for dense feature only
  struct {
    float value;
    uint32_t dense_sign;
};

/// 基类
class FastFeature : public FastFeatureInterface {
 public:
  explicit FastFeature(FeatureType type, size_t left_bits = 52) :
    FastFeatureInterface(type, left_bits) {}

  virtual ~FastFeature() {}

  void ExtractFea(const AdLogInterface& adlog, size_t pos,
                  std::vector<ExtractResult>* result) {
    return Extract(reinterpret_cast<const AdLog&>(adlog), pos, result);
  }

 protected:
  virtual void Extract(const AdLog& adlog, size_t pos,
                       std::vector<ExtractResult>* result) = 0;
};

/// 子类: 用户 id 特征提取类。
class ExtractUserId : public FastFeature {
 public:
  ExtractUserId():FastFeature(FeatureType::USER){}
  virtual void Extract(const AdLog& adlog,
                       size_t pos,
                       std::vector<ExtractResult>* result) {
    if(adlog.has_user_info()){
      AddFeature(GetFeature(FeaturePrefix::USER_ID,adlog.user_info().id()),1.0f,result);
    }
  }
};
REGISTER_EXTRACTOR(ExtractUserId);
```

注意: `AdLog` 是对 `protobuf` 定义的 `AdJointLabeledLog` 的简单封装。
