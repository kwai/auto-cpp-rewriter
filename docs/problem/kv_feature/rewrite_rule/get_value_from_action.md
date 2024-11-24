# get_value_from_action

通用的函数，会在 `add_feature` 中被调用。参数固定。

共 11个类，见:

- teams/ad/ad_algorithm/feature/fast/impl/extract_user_ad_click_no_lps.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_user_ad_item_click_no_conv.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_user_ad_item_click_no_lps.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_user_ad_played3s_no_item_click.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_user_ad_played3s_no_lps.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_user_ad_played5s_no_item_click.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_user_ad_played5s_no_lps.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_user_ad_playedend_no_lps.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_user_ad_played_fix5s_no_item_click.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_user_ad_server_no_item_click_flag.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_user_ad_server_no_item_click.h

示例: teams/ad/ad_algorithm/feature/fast/impl/extract_user_ad_click_no_lps.h

```cpp
    if (adlog.has_user_info() && adlog.user_info().explore_long_term_ad_action_size() > 0) {
      const auto& ad_action = adlog.user_info().explore_long_term_ad_action();
      auto item_played5s_iter = ad_action.find(item_played5s_no);
      auto item_click_iter = ad_action.find(item_click_no);
      if (item_played5s_iter != ad_action.end() && item_click_iter != ad_action.end()) {
        const auto& item_played5s_action_base_infos = item_played5s_iter->second.list();
        const auto& item_click_action_base_infos = item_click_iter->second.list();

        add_feature(item_played5s_action_base_infos, item_click_action_base_infos,
            FeaturePrefix::USER_AD_CLICK_NO_LPS_PRODUCT_1,
            FeaturePrefix::USER_AD_CLICK_NO_LPS_INDUSTRY_1, 24, process_time, result);

        add_feature(item_played5s_action_base_infos, item_click_action_base_infos,
            FeaturePrefix::USER_AD_CLICK_NO_LPS_PRODUCT_3,
            FeaturePrefix::USER_AD_CLICK_NO_LPS_INDUSTRY_3, 72, process_time, result);

        add_feature(item_played5s_action_base_infos, item_click_action_base_infos,
            FeaturePrefix::USER_AD_CLICK_NO_LPS_PRODUCT_7,
            FeaturePrefix::USER_AD_CLICK_NO_LPS_INDUSTRY_7, 168, process_time, result);

        add_feature(item_played5s_action_base_infos, item_click_action_base_infos,
            FeaturePrefix::USER_AD_CLICK_NO_LPS_PRODUCT_30,
            FeaturePrefix::USER_AD_CLICK_NO_LPS_INDUSTRY_30, 720, process_time, result);
      }
    }
```
