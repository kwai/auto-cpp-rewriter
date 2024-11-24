# normal_adlog_field

普通 `adlog` 字段，指最终叶子节点是简单的类型，中间节点没有复杂类型的情况，如 `adlog.user_info.id` 等。
直接通过字段路径名对应 `bslog` 中的路径，然后根据 `proto` 字段类型对应 `bslog` 中的类型。

示例: teams/ad/ad_algorithm/feature/fast/impl/extract_photo_video_feature_moco_cluster.h

```cpp
  auto &item = adlog.item(pos);
  if ((item.type() == AD_DSP || item.type() == NATIVE_AD) && item.has_ad_dsp_info()
    && item.ad_dsp_info().common_info_attr_size() > 0) {
    ...
  }
```

`adlog.item.type` 则表示 `item type` 字段，类型是枚举，可以转换成 `int` 类型。

## 改写规则

这种类型的字段是最简单的类型，只需要在解析时候根据表达式本身就可以确定对应的改写逻辑。

`bslog` 特征类改写示例: teams/ad/ad_algorithm/bs_feature/fast/impl/bs_extract_photo_video_feature_moco_cluster.cc

```cpp
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
```
