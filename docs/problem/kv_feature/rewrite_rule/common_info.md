# common info

## 普通 common info

遍历 `common_info_attr` 列表，通过枚举判断是否是需要的数据，并在处理结束后 `break` 循环。

示例: teams/ad/ad_algorithm/feature/fast/impl/extract_photo_video_feature_moco_cluster.h

```cpp
  for (const auto &attr : item.ad_dsp_info().common_info_attr()) {
    if (attr.name_value() == ::auto_cpp_rewriter::CommonInfoAttr_Name_VIDEO_MOCO_CLUSTER) {
      AddFeature(GetFeature(FeaturePrefix::PHOTO_VIDEO_MOCO_CLUSTER, attr.int_value()), 1.0f, result);
      break;
    }
  }
```

### 改写规则

`bslog` 特征了改写示例: teams/ad/ad_algorithm/bs_feature/fast/impl/bs_extract_photo_video_feature_moco_cluster.cc

```cpp
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

## 来自中间节点的 common info

示例: teams/ad/ad_algorithm/feature/fast/impl/extract_item_goods_id_list_size.h, 所需字段来自中间
节点的 common info, 如 live info common info。

```cpp
  auto live_info = GetLiveInfo(adlog.item(pos));
  if (live_info == nullptr) {
    return;
  }
  if (live_info->common_info_attr_size() > 0) {
    const auto& attr = live_info->common_info_attr();
    for (const auto& liveAttr : attr) {
      if (liveAttr.name_value() == 23071) {
        int goods_num = liveAttr.int_list_value_size();
        AddFeature(0, goods_num, result);
        break;
      }
    }
  }
```

在遇到 `common_info_attr` 时候可以确定是否来自中间节点, 如果是则创建 `CommonInfoMiddleNode`, 之后需要
重新实现 `CommonInfo` 对应的逻辑。

注意，有可能同时出现中间节点和 `common info`, 但是 `common info` 不是来自于中间节点。需要在创建
`common_info_normal` 或者 `common_info_fixed_list` 时候根据 `prefix_adlog` 是否以 `adlog` 开头来区分。

示例:

- teams/ad/ad_algorithm/feature/fast/impl/extract_combine_cross_eds_comp_product_name_clk.h

## common info case switch

见:

- teams/ad/ad_algorithm/feature/fast/impl/extract_user_ad_impression_realtime_new_extend.h

示例: teams/ad/ad_algorithm/feature/fast/impl/extract_live_realtime_audience_count.h

    for (int i = 0; i < live_info->common_info_attr_size(); i++) {
      const ::auto_cpp_rewriter::CommonInfoAttr& attr =
          live_info->common_info_attr(i);
      if (attr.type() == ::auto_cpp_rewriter::CommonTypeEnum_AttrType_INT_ATTR) {
        switch (attr.name_value()) {
          case ::auto_cpp_rewriter::
              CommonInfoAttr_Name_LIVE_ACCUMULATIVE_START_PLAY_COUNT:
            play_cnt_acc = attr.int_value();
          case ::auto_cpp_rewriter::
              CommonInfoAttr_Name_LIVE_ACCUMULATIVE_STOP_PLAY_COUNT:
            stop_play_cnt_acc = attr.int_value();
        }
      }
    }
    
根据 switch 的条件将 enum 的值添加到 `CommonInfoNormal` 中即可。需要处理共用逻辑的情况。

## GetCommonInfoAttrNew

这种写法比较绕, 共有 5 个类是这种写法, 而且其中两个 `switch` 语句有 bug, 忽略, 手动改。

- teams/ad/ad_algorithm/feature/fast/impl/extract_photo_author_level_id_new.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_live_author_tag_category_new_fix.cc

其他三个是

- teams/ad/ad_algorithm/feature/fast/impl/extract_fanstop_photo_mmu_hetu_tag_new.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_live_author_tag_category_new.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_photo_enhance_htag_new.h

## common info 模板参数

`common_info` 枚举来自模板参数。且逻辑中没有 `break`。

以下类是这种写法, 共有 19 个。

- teams/ad/ad_algorithm/feature/fast/impl/extract_user_realtime_action.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_user_realtime_action_time_stamp.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_combine_applist_sdpa_info.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_combine_user_info_sdpa_info.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_dense_sdpa_ecom_pid_action_cnt.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_dense_sdpa_list_cnt.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_sdpa_ecom_user_action_kwds.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_u_realtime_action_len.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_u_realtime_action_list_add.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_u_realtime_action_list.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_u_realtime_action_seq.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_u_realtime_action_sp_list.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_u_realtime_action_ts_seq.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_user_dsplive_realtime_action_list.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_user_search_action_list.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_user_search_action_list_v2.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_user_search_action_timestamp_list.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_user_search_action_timestamp_list_v2.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_u_spu_action_list.h
  

示例: teams/ad/ad_algorithm/feature/fast/impl/extract_user_realtime_action.h

    if (adlog.has_user_info() && adlog.user_info().common_info_attr_size() > 0) {
      for (const ::auto_cpp_rewriter::CommonInfoAttr& user_attr : adlog.user_info().common_info_attr()) {
        if (user_attr.name_value() == user_attr_name_) {
          for (const auto& val : user_attr.int_list_value()) {
            action_list.push_back(val);
          }
        }
        if (user_attr.name_value() == timestamp_attr_name_) {
          for (const auto& val : user_attr.int_list_value()) {
            timestamp_list.push_back(val);
          }
        }
        if (action_list.size() > 0 && timestamp_list.size() > 0) {
          break;
        }
      }
    }

其中 `user_attr_name_` 和 `timestamp_attr_name_` 是模板参数。

用 `CommonInfoFixed` 处理。

## common info 放到 map 里

`common info` 用 `map` 存起来，用 `CommonInfoMultiIntList` 表示。

以下类是这种写法:

- teams/ad/ad_algorithm/feature/fast/impl/extract_user_rtl_product_name_shallow_action_7d.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_user_rtl_photo_id_deep_action_30d.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_user_rtl_photo_id_shallow_action_3d.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_user_rtl_photo_id_shallow_action_7d.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_user_rtl_product_name_deep_action_90d.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_user_rtl_product_name_shallow_action_30d.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_user_rtl_product_name_deep_action_30d.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_user_rtl_photo_id_deep_action_90d.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_user_rtl_photo_id_shallow_action_3d.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_user_rtl_product_name_shallow_action_30d.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_user_rtl_industry_id_shallow_action_30d.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_user_rtl_photo_id_deep_action_90d.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_user_rtl_industry_id_deep_action_90d.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_user_rtl_industry_id_shallow_action_3d.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_user_rtl_industry_id_shallow_action_7d.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_user_rtl_product_name_shallow_action_7d.h

示例:

      // 定义 map 字段 空间换时间
      std::unordered_map<int,
        const google::protobuf::RepeatedField<::google::protobuf::int64>*> action_name2list;
      std::unordered_map<int,
        const google::protobuf::RepeatedField<::google::protobuf::int64>*> timestamp_name2list;
      std::unordered_map<int, int> action_name2size;
      std::unordered_map<int, int> timestamp_name2size;

      // 遍历用户行为
      for (const ::auto_cpp_rewriter::CommonInfoAttr &userAttr : adlog.user_info().common_info_attr()) {
        if (userAttr.name_value() < action_map.size() && action_map[userAttr.name_value()]) {
          action_name2list[userAttr.name_value()] = &(userAttr.int_list_value());
          action_name2size[userAttr.name_value()] = userAttr.int_list_value_size();
        }

        if (userAttr.name_value() < timestamp_map.size() && timestamp_map[userAttr.name_value()]) {
          timestamp_name2list[userAttr.name_value()] = &(userAttr.int_list_value());
          timestamp_name2size[userAttr.name_value()] = userAttr.int_list_value_size();
        }
      }

      // 截断用户行为和行为时间
      for (int i = 0; i < action_vec.size(); i ++) {
        auto action_name = action_vec[i];
        auto timestamp_name = timestamp_vec[i];
        ...
      }
      
    
`CommonInfo` 结果保存在 `action_name2list` 和 `timestamp_name2list` 中。`common_info` 枚举通过构造函数添加到 
`action_vev` 和 `timestamp_vec` 中。

先通过 `OverviewHandler` 找到 `common_info` 的 `prefix`, 然后统一替换 `map` 相关操作。通过 `if` 条件中国的
`name_value()` 和 `action_map` 来判断。需要用 `CommonInfoFixed`。

## 遍历 common info map_int_int

以下类是这种写法:

- teams/ad/ad_algorithm/feature/fast/impl/extract_combine_user_applist_cate_match_num.h

## 遍历 common info int_list

见:

- teams/ad/ad_algorithm/feature/fast/impl/extract_user_eshop_click_item_cate3_id.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_user_photo_click_cart_seller_id_list.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_user_merchant_click_cart_seller_id_list.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_user_merchant_buy_item_id_list.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_user_eshop_click_item_cate2_id.h

注意有强转成 `int` 的情况。

## common info int_list 强转 int

- teams/ad/ad_algorithm/feature/fast/impl/extract_live_user_merchant_list_online.h

### 同时出现 common info normal 和 common info fixed

同时出现 common info normal 和 common info fixed。

见:
- teams/ad/ad_algorithm/feature/fast/impl/extract_combine_author_media_app_avg_explore_ratio.h

之前的逻辑是只能出现一种，在 overview 中会进行标记是哪一种。通过如下逻辑判断是否是 `common info fixed':

    template_int_names_.size() > 0 && common_info_values_.size() == 0

因此需要想办法支持两种都存在的情况。
