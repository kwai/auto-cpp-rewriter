## reco_user_info

示例: teams/ad/ad_algorithm/feature/fast/impl/extract_reco_user_like_list.h

只需要替换 `adlog.has_reco_user_info()` 和 `adlog.reco_user_info()`, 其他逻辑都不用动。

    if (!adlog.has_reco_user_info()) {
      return;
    }

    const auto *ruinfo = adlog.reco_user_info();