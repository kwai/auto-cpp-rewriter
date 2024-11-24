#pragma once

#include <glog/logging.h>
#include <absl/types/optional.h>

#include "clang/AST/Expr.h"

#include "RuleBase.h"
#include "../Type.h"
#include "../handler/StrictRewriter.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

class Env;

/// 两层 list。遍历到第二层 for 循环时候才确定是两层 list。
/// 如 teams/ad/ad_algorithm/feature/fast/impl/extract_combine_user_app_list_ad_app_id.h
/// for ( auto & device_info : device_info_list ) {
///   auto& app_package_list = device_info.app_package();
///   for ( auto & app_package : app_package_list ) {
///     AddFeature(GetFeature(FeaturePrefix::COMBINE_USER_APP_LIST_AND_AD_APPID,
///                           base::CityHash64(app_package.data(),
///                                            app_package.size()), app_id), 1.0, result);
///   }
/// }
class DoubleListRule: public RuleBase {
 public:
  using RuleBase::process;
  explicit DoubleListRule(clang::Rewriter& rewriter): RuleBase(rewriter, "DoubleListRule") {}  // NOLINT

  void process(clang::ForStmt* for_stmt,
               Env* env_ptr) override;

  void process(clang::CXXForRangeStmt* cxx_for_range_stmt,
               Env* env_ptr) override;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
