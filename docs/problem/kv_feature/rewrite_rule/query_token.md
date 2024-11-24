# query_token

## GetQueryToken

common info map_string_float。
将函数替换为直接返回 `BSMapField`, 再替换 `for` 循环或者其他引用的部分。

有 20 个类用了 `GetQueryToken`。

见:

- teams/ad/ad_algorithm/feature/fast/impl/extract_combine_querytoken_ad_campaign_id.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_combine_querytoken_ad_campaign_type.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_combine_querytoken_ad_industry.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_combine_querytoken_ad_unit_id.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_combine_querytoken_photo.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_query_token.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_search_photo_asr_2.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_search_photo_asr.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_search_photo_cname_2.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_search_photo_cname.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_search_photo_description_2.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_search_photo_description.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_search_photo_ocr_2.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_search_photo_ocr.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_search_photo_ocr_title_2.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_search_photo_ocr_title.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_search_photo_pname_2.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_search_photo_pname.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_search_photo_slogan_2.h
- teams/ad/ad_algorithm/feature/fast/impl/extract_search_photo_slogan.h
