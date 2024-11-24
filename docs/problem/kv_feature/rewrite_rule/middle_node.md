# middle_node

字段来自 `PhotoInfo` 或者 `AuthorInfo` 等中间嵌套节点。

示例: teams/ad/ad_algorithm/feature/fast/impl/extract_combine_follow_author_id_v1.h

```cpp
  auto live_info = GetLiveInfo(adlog.item(pos));

  if(live_info == nullptr){
    return;
  }

  if(!live_info->has_author_info() ) {
    return;
  }

 . const auto & author_info = live_info->author_info();
  if(!adlog.has_user_info()
      ||!adlog.user_info().has_action_detail()) {
    return;
  }

  const auto & action_detail = adlog.user_info().action_detail();
  int follow_num = action_detail.follow().size();

  char author_buffer[256];
  char follow_buffer[256];

  for(int i = 0; i < follow_num && i < max_follow_num; ++i){
    const auto & follow_info = action_detail.follow(i);

    if(follow_info.id() == 90041) {
      continue;
    }

    if(follow_info.id() == author_info.id()) {
      continue;
    }

    uint64_t id = GetFeature(FeaturePrefix::COMBINE_FOLLOW_AUTHOR_NEW,
                             follow_info.id());
    AddFeature(id, 1.0, result);
  }
```

叶子节点也可能是 str 或者 list。

可能最早遇到 `size` 方法，此时知道去掉 `size` 后 `bs_enum_str` 对应的变量名，然后替换。需要在
`OverviewHandler` 中记录叶子节点的类型，用来创建变量。

## 改写规则

根据叶子节点对应到提前实现好的模板类，具体实现逻辑见: teams/ad/ad_algorithm/bs_feature/fast/frame/bs_info_util.h