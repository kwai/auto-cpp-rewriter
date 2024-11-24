#include "ActionMethodInfo.h"
#include "NewActionParam.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

const std::string ActionMethodInfo::name_ = "ks::ad_algorithm::get_value_from_Action";

ActionMethodInfo::ActionMethodInfo(size_t param_size): param_size_(param_size) {
  new_action_params_.emplace_back("first_list");
  new_action_params_.emplace_back("second_list");

  // 注意，必须用 uint64, 否则 cdiff 不过。
  NewActionFieldParam param0_timestamp("first_list_action_timestamp",
                                       "action_timestamp",
                                       "uint64_t",
                                       false);
  new_action_params_[0].add_new_param(param0_timestamp);

  NewActionFieldParam param0_product_id_hash("first_list_product_id_hash",
                                             "product_id_hash",
                                             "uint64_t",
                                             false);
  new_action_params_[0].add_new_param(param0_product_id_hash);

  NewActionFieldParam param0_second_industry_id_hash("first_list_second_industry_id_hash",
                                                     "second_industry_id_hash",
                                                     "uint64_t",
                                                     false);
  new_action_params_[0].add_new_param(param0_second_industry_id_hash);

  if (param_size_ == 10) {
    NewActionFieldParam param0_photo_id("first_list_photo_id",
                                        "photo_id",
                                        "uint64_t",
                                        false);
    new_action_params_[0].add_new_param(param0_photo_id);
  }

  NewActionFieldParam param1_timestamp("second_list_action_timestamp",
                                       "action_timestamp",
                                       "uint64_t",
                                       false);
  new_action_params_[1].add_new_param(param1_timestamp);

  NewActionFieldParam param1_product_id_hash("second_list_product_id_hash",
                                             "product_id_hash",
                                             "uint64_t",
                                             false);
  new_action_params_[1].add_new_param(param1_product_id_hash);

  NewActionFieldParam param1_second_industry_id_hash("second_list_second_industry_id_hash",
                                                     "second_industry_id_hash",
                                                     "uint64_t",
                                                     false);
  new_action_params_[1].add_new_param(param1_second_industry_id_hash);

  if (param_size_ == 10) {
    NewActionFieldParam param1_photo_id("second_list_photo_id",
                                        "photo_id",
                                        "uint64_t",
                                        false);
    new_action_params_[1].add_new_param(param1_photo_id);
  }
}

const NewActionParam& ActionMethodInfo::find_param(size_t index) const {
  if (index >= new_action_params_.size()) {
    static NewActionParam empty;
    return (empty);
  }

  return (new_action_params_[index]);
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
