#include "../Tool.h"
#include "PrefixPair.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

PrefixPair::PrefixPair(const std::string& prefix_adlog) {
  prefix_adlog_ = prefix_adlog;
  prefix_ = tool::adlog_to_bs_enum_str(prefix_adlog);
  LOG(INFO) << "prefix_: " << prefix_
            << ", prefix_adlog_: " << prefix_adlog;
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
