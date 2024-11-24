# 特征改写

我们用 `bslog` 表示 `flatbuffers` 格式的 `kv` 数据。


我们用一个示例来展示如何将 `protobuf` 格式的特征类改写为 `flatbuffers` 格式的特征类。

`adlog` 特征类

```c++
class ExtractPhotoVideoFeatureMocoCluster : public FastFeature {
 public:
  ExtractPhotoVideoFeatureMocoCluster()
      :FastFeature(ITEM) {
  }

  virtual void Extract(const AdLog & adlog, size_t pos, std::vector<ExtractResult>* result) {
    if (pos >= adlog.item_size()) {
      return;
    }

    auto &item = adlog.item(pos);
    if ((item.type() == AD_DSP || item.type() == NATIVE_AD) && item.has_ad_dsp_info()
      && item.ad_dsp_info().common_info_attr_size() > 0) {
      for (const auto &attr : item.ad_dsp_info().common_info_attr()) {
        if (attr.name_value() == ::auto_cpp_rewriter::CommonInfoAttr_Name_VIDEO_MOCO_CLUSTER) {
          AddFeature(GetFeature(FeaturePrefix::PHOTO_VIDEO_MOCO_CLUSTER, attr.int_value()), 1.0f, result);
          break;
        }
      }
    }
  }

 private:
  const std::string USED_FEATURES[1] = {
    "item.ad_dsp_info.common_info_attr.VIDEO_MOCO_CLUSTER"
  };
  DISALLOW_COPY_AND_ASSIGN(ExtractPhotoVideoFeatureMocoCluster);
};

REGISTER_EXTRACTOR(ExtractPhotoVideoFeatureMocoCluster);
```


`bslog` 特征类

```c++

// .h
class BSExtractPhotoVideoFeatureMocoCluster : public BSFastFeature {
 public:
  BSExtractPhotoVideoFeatureMocoCluster();

  virtual void Extract(const BSLog& bslog, size_t pos, std::vector<ExtractResult>* result);

 private:
  DISALLOW_COPY_AND_ASSIGN(BSExtractPhotoVideoFeatureMocoCluster);
};

REGISTER_BS_EXTRACTOR(BSExtractPhotoVideoFeatureMocoCluster);


// .cc
BSExtractPhotoVideoFeatureMocoCluster::BSExtractPhotoVideoFeatureMocoCluster() : BSFastFeature(ITEM) {
  attr_metas_.emplace_back(BSFieldEnum::adlog_item_type);
  attr_metas_.emplace_back(BSFieldEnum::adlog_item_ad_dsp_info_exists);
  attr_metas_.emplace_back(BSFieldEnum::adlog_item_ad_dsp_info_common_info_attr_size);
  attr_metas_.emplace_back(BSFieldEnum::adlog_item_ad_dsp_info_common_info_attr_key_535);
}

void BSExtractPhotoVideoFeatureMocoCluster::Extract(const BSLog& bslog, size_t pos,
                                                    std::vector<ExtractResult>* result) {
  auto bs = bslog.GetBS();
  if (bs == nullptr) {
    return;
  }

  auto enum_attr_size = BSFieldEnum::adlog_item_ad_dsp_info_common_info_attr_size;
  int32_t attr_size = BSFieldHelper::GetSingular<int32_t>(*bs, enum_attr_size, pos);

  auto enum_info_exists = BSFieldEnum::adlog_item_ad_dsp_info_exists;
  bool info_exists = BSFieldHelper::GetSingular<bool>(*bs, enum_info_exists, pos);

  auto enum_item_type = BSFieldEnum::adlog_item_type;
  int64_t item_type = BSFieldHelper::GetSingular<int64_t>(*bs, enum_item_type, pos);

  auto enum_key_535 = BSFieldEnum::adlog_item_ad_dsp_info_common_info_attr_key_535;
  int64_t key_535 = BSFieldHelper::GetSingular<int64_t>(*bs, enum_key_535, pos);

  auto enum_key_535_exists = BSFieldEnum::adlog_item_ad_dsp_info_common_info_attr_key_535;
  bool key_535_exists = BSFieldHelper::HasSingular<int64_t>(*bs, enum_key_535_exists, pos);

  if ((item_type == bs::ItemType::AD_DSP || item_type == bs::ItemType::NATIVE_AD) && info_exists &&
      attr_size > 0) {
    if (key_535_exists) {
      AddFeature(GetFeature(FeaturePrefix::PHOTO_VIDEO_MOCO_CLUSTER, key_535), 1.0f, result);
    }
  }
}
```

## 改写步骤

以上示例是简单的 `CommonInfoAttr` 类型。

在 `adlog` 特征类中，我们获取的数据来自 `AdLog`, 需要遍历 `common_info_attr` 列表，并根据 `name_value`
来判断是否是我们需要的特征。如果找到了对应的数据，在处理完成后则需要 `break`。这是处理 `CommonInfoAttr` 类型
特征的固定写法。

```c++
for (const auto &attr : item.ad_dsp_info().common_info_attr()) {
  if (attr.name_value() == ::auto_cpp_rewriter::CommonInfoAttr_Name_VIDEO_MOCO_CLUSTER) {
    AddFeature(GetFeature(FeaturePrefix::PHOTO_VIDEO_MOCO_CLUSTER, attr.int_value()), 1.0f, result);
    break;
  }
}
```

而在 `bslog` 特征类中，我们为了和 `adlog` 特征类对齐逻辑，获取数据的写法需要修改为按 `key` 的方式获取，并且还需要
判断 `key` 是否存在，以对齐 `adlog` 中列表元素是否存在的逻辑。可以按如下步骤进行改写：

1. 根据字段路径获取 `key` 的值, 以及字段是否存在的结果。

    ```c++
    auto enum_key_535 = BSFieldEnum::adlog_item_ad_dsp_info_common_info_attr_key_535;
    int64_t key_535 = BSFieldHelper::GetSingular<int64_t>(*bs, enum_key_535, pos);

    auto enum_key_535_exists = BSFieldEnum::adlog_item_ad_dsp_info_common_info_attr_key_535;
    bool key_535_exists = BSFieldHelper::HasSingular<int64_t>(*bs, enum_key_535_exists, pos);
    ```

2. 直接用 `key` 获取的结果来实现计算逻辑即可，不需要 `for` 循环遍历。
3. 构造函数中添加 `bs` 字段的路径。
4. 修改注册特征的宏。

按以上步骤改写后，则可以和之前的逻辑完全对齐。

还有其他类型的特征，如获取 `ActionDetail` 中的数据、`PhotoInfo` 等中间节点的数据、模板等等, 由于获取 `protobuf`
格式的数据逻辑和计算逻辑耦合比较严重，因此每中类型的特征都需要专门的处理逻辑。

具体示例可见 `teams/ad/ad_algorithm/feature/fast/impl/` 目录下的特征类，对应的改写后的 `bslog` 特征类在
`teams/ad/ad_algorithm/bs_feature/fast/impl/` 目录下。