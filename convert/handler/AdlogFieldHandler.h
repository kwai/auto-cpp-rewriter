#pragma once

#include <glog/logging.h>
#include <gflags/gflags.h>

#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Basic/SourceLocation.h"

#include "../Tool.h"
#include "../Env.h"
#include "../rule/PreRule.h"
#include "../rule/GeneralRule.h"
#include "../rule/CommonInfoRule.h"
#include "../rule/MiddleNodeRule.h"
#include "../rule/ActionDetailRule.h"
#include "../rule/SeqListRule.h"
#include "../rule/DoubleListRule.h"
#include "../rule/ProtoListRule.h"
#include "../rule/AddFeatureMethodRule.h"
#include "../rule/HashFnRule.h"
#include "../rule/QueryTokenRule.h"
#include "../rule/StrRule.h"
#include "StrictRewriter.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class ExprInfo;

class AdlogFieldHandler {
 public:
  explicit AdlogFieldHandler(clang::Rewriter& rewriter):    // NOLINT
    rewriter_(rewriter),
    pre_rule_(rewriter),
    general_rule_(rewriter),
    common_info_rule_(rewriter),
    action_detail_rule_(rewriter),
    middle_node_rule_(rewriter),
    seq_list_rule_(rewriter),
    double_list_rule_(rewriter),
    proto_list_rule_(rewriter),
    add_feature_method_rule_(rewriter),
    hash_fn_rule_(rewriter),
    query_token_rule_(rewriter),
    str_rule_(rewriter) {}

  template<typename T>
  void process(T t, Env* env_ptr) {
    pre_rule_.process(t, env_ptr);

    common_info_rule_.process(t, env_ptr);
    middle_node_rule_.process(t, env_ptr);
    action_detail_rule_.process(t, env_ptr);
    double_list_rule_.process(t, env_ptr);
    seq_list_rule_.process(t, env_ptr);
    proto_list_rule_.process(t, env_ptr);
    add_feature_method_rule_.process(t, env_ptr);
    hash_fn_rule_.process(t, env_ptr);
    query_token_rule_.process(t, env_ptr);
    str_rule_.process(t, env_ptr);

    /// 由于一些插入变量等逻辑, general_rule_ 必须放到最后一个。
    general_rule_.process(t, env_ptr);
  }

 private:
  StrictRewriter rewriter_;

  PreRule pre_rule_;

  CommonInfoRule common_info_rule_;

  MiddleNodeRule middle_node_rule_;

  ActionDetailRule action_detail_rule_;

  DoubleListRule double_list_rule_;

  SeqListRule seq_list_rule_;

  ProtoListRule proto_list_rule_;

  AddFeatureMethodRule add_feature_method_rule_;

  HashFnRule hash_fn_rule_;

  QueryTokenRule query_token_rule_;

  StrRule str_rule_;

  GeneralRule general_rule_;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
