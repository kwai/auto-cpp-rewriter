## 多个 action 通过数组保存

示例: teams/ad/ad_algorithm/feature/fast/impl/extract_match_dense_num.h

    const auto& ad_action = adlog.user_info().explore_long_term_ad_action();
    for (auto action_no : action_vec_) {
      auto action_no_iter = ad_action.find(action_no);
      int photo_id_action_num = 0;
      int product_name_hash_action_num = 0;
      int second_industy_id_hash_action_num = 0;
      if (action_no_iter != ad_action.end()) {
        const auto& action_no_list = action_no_iter->second.list();
        for (int k = 0; k < action_no_list.size() && k < 100; ++k) {
          ...
        }
    }
    
用第一个进行处理，然后将其他 action 转换为 lambda 函数, 具体处理逻辑见 `ActionDetailRule`。
