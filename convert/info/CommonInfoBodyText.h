#pragma once

#include <glog/logging.h>
#include <absl/types/optional.h>

#include <set>
#include <map>
#include <string>
#include <unordered_set>

namespace ks {
namespace ad_algorithm {
namespace convert {

class CommonInfoBodyText {
 public:
  void set_bs_rewritten_text(const std::string &bs_pre_text,
                             const std::string &bs_loop_text,
                             const std::string &bs_post_text);
  void set_bs_rewritten_text(const CommonInfoBodyText& body_text);

  const absl::optional<std::string> &bs_loop_text() const {return (bs_loop_text_);}
  const absl::optional<std::string> &bs_post_text() const {return (bs_post_text_);}

  const std::string &get_bs_pre_text() const { return (bs_pre_text_); }
  const std::string& get_bs_loop_text() const;
  const std::string& get_bs_post_text() const;

  std::string bs_body_text() const;

 protected:
  std::string bs_pre_text_;
  absl::optional<std::string> bs_loop_text_;
  absl::optional<std::string> bs_post_text_;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
