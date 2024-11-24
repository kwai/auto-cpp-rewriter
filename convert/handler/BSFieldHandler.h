#pragma once

#include <nlohmann/json.hpp>
#include <glog/logging.h>
#include <gflags/gflags.h>

#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Basic/SourceLocation.h"

#include "../Tool.h"
#include "../Env.h"
#include "./StrictRewriter.h"
#include "../rule/BSFieldOrderRule.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

using nlohmann::json;

class ExprInfo;

class BSFieldHandler {
 private:
  StrictRewriter rewriter_;
  BSFieldOrderRule bs_field_order_rule_;

 public:
  explicit BSFieldHandler(clang::Rewriter& rewriter):    // NOLINT
    rewriter_(rewriter),
    bs_field_order_rule_(rewriter) {}

  template<typename T> json process_to_json(T t, Env* env_ptr) {
    bs_field_order_rule_.process_to_json(t, env_ptr);

    return json::array();
  }
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
