# action_detail

`action_detail` 在 `proto` 的 `user_info` 中是以 `map` 形式存储的，`key` 是 `action number`, `value`
是嵌套的 `UserActionDetail` 类型。

使用时需要先根据 `action number` 找到对应的 `value`, 然后再根据嵌套的字段取得具体的数据。

示例: teams/ad/ad_algorithm/feature/fast/impl/extract_combine_longterm_action_cnt_ts_p5s_fix.h

```cpp
  int32_t action = 4;

  const auto& ad_dsp_action_detail = adlog.user_info().explore_long_term_ad_action();

  auto iter = ad_dsp_action_detail.find(action);
  if (iter == ad_dsp_action_detail.end()) {
    return;
  }

  const auto& dsp_infos = iter->second.list();
  ...

  for (int i = 0; i < dsp_infos.size() && i < 1000; ++i) {
    auto& dinfo = dsp_infos.Get(i);
    uint64 atime = dinfo.action_timestamp();

    if (photo_id == dinfo.photo_id()) {
      ...
    }

    if (prod_id == dinfo.product_id_hash()) {
      ...
    }
  }
```

## 改写规则

需要先根据后面的代码找到具体的叶子节点字段，然后记录到 `Env` 中，之后对每个字段进行 `bslog` 数据的获取，将遍历
逻辑替换为直接遍历具体某个字段的逻辑。`map` 查找相关的逻辑也直接替换为根据路径 `key` 获取数据的逻辑。

`bslog` 特征类改写示例: teams/ad/ad_algorithm/bs_feature/fast/impl/bs_extract_combine_longterm_action_cnt_ts_p5s_fix.cc

```cpp
  auto enum_ctime = BSFieldEnum::adlog_time;
  uint64_t ctime = BSFieldHelper::GetSingular<uint64_t, true>(*bs, enum_ctime, pos);

  auto enum_info_exists = BSFieldEnum::adlog_user_info_exists;
  bool info_exists = BSFieldHelper::GetSingular<bool, true>(*bs, enum_info_exists, pos);

  auto enum_base_exists = BSFieldEnum::adlog_item_ad_dsp_info_advertiser_base_exists;
  bool base_exists = BSFieldHelper::GetSingular<bool>(*bs, enum_base_exists, pos);

  auto enum_creative_id = BSFieldEnum::adlog_item_ad_dsp_info_creative_base_id;
  uint64_t creative_id = BSFieldHelper::GetSingular<uint64_t>(*bs, enum_creative_id, pos);

  auto enum_photo_id = BSFieldEnum::adlog_item_ad_dsp_info_creative_base_photo_id;
  uint64_t photo_id = BSFieldHelper::GetSingular<uint64_t>(*bs, enum_photo_id, pos);

  auto enum_action_timestamp =
      BSFieldEnum::adlog_user_info_explore_long_term_ad_action_key_4_list_action_timestamp;
  BSRepeatedField<uint64_t, true> action_timestamp(*bs, enum_action_timestamp, pos);

  auto enum_list_photo_id = BSFieldEnum::adlog_user_info_explore_long_term_ad_action_key_4_list_photo_id;
  BSRepeatedField<uint64_t, true> list_photo_id(*bs, enum_list_photo_id, pos);

  auto enum_id_hash = BSFieldEnum::adlog_user_info_explore_long_term_ad_action_key_4_list_product_id_hash;
  BSRepeatedField<uint64_t, true> id_hash(*bs, enum_id_hash, pos);

  auto enum_hash_v3 =
      BSFieldEnum::adlog_user_info_explore_long_term_ad_action_key_4_list_second_industry_id_hash_v3;
  BSRepeatedField<uint64_t, true> hash_v3(*bs, enum_hash_v3, pos);

  auto enum_list_size = BSFieldEnum::adlog_user_info_explore_long_term_ad_action_key_4_list_size;
  int32_t list_size = BSFieldHelper::GetSingular<int32_t, true>(*bs, enum_list_size, pos);

  auto enum_list_size_exists = BSFieldEnum::adlog_user_info_explore_long_term_ad_action_key_4_list_size;
  bool list_size_exists = BSFieldHelper::HasSingular<int64_t, true>(*bs, enum_list_size_exists, pos);

  if (info_exists) {
    // 组合一下

    uint64 prod_id = 0;
    auto enum_pname = BSFieldEnum::adlog_item_ad_dsp_info_advertiser_base_product_name;
    absl::string_view pname = BSFieldHelper::GetSingular<absl::string_view>(*bs, enum_pname, pos);

    if (base_exists) {
      prod_id = base::CityHash64(pname.data(), pname.size());
    }
    uint64 secind_id = 0;
    auto enum_iname = BSFieldEnum::adlog_item_ad_dsp_info_advertiser_base_second_industry_name;
    absl::string_view iname = BSFieldHelper::GetSingular<absl::string_view>(*bs, enum_iname, pos);

    if (base_exists) {
      secind_id = base::CityHash64(iname.data(), iname.size());
    }
    bool key_exists = list_size_exists;
    if (FLAGS_use_exist_fix_version) {
      key_exists = BSFieldHelper::GetSingular<bool, true>(
          *bs, BSFieldEnum::adlog_user_info_explore_long_term_ad_action_key_4_exists, pos);
    }
    if (!key_exists) {
      return;
    }

    const int C = 6;
    const int TC = 11;
    int ccnts[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int pcnts[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int ucnts[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    for (int i = 0; i < list_size && i < 1000; ++i) {
      double days = (ctime - action_timestamp.Get(i)) / 1000 / 3600 / 24;
      int idx = 0;
      if (days < 3) {
        idx = 0;
      } else if (days >= 3 && days < 7) {
        idx = 1;
      } else if (days >= 7 && days < 15) {
        idx = 2;
      } else if (days >= 15 && days < 30) {
        idx = 3;
      } else if (days >= 30 && days < 90) {
        idx = 4;
      } else {
        idx = 5;
      }
      if (photo_id == list_photo_id.Get(i)) {
        ccnts[idx] += 1;
      }
      if (prod_id == id_hash.Get(i)) {
        pcnts[idx] += 1;
      }
      if (secind_id == hash_v3.Get(i)) {
        ucnts[idx] += 1;
      }
    }
  }
```
