# photo_text

## GetPhotoText

类似 `GetQueryToken` 的逻辑, 不过多了 `pos` 和 `common info enum` 这两个参数。需要用到
`FixedCommonInfo` 。

有 14 个类用了 `GetPhotoText`。

见:

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
