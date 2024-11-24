#include <regex>
#include <sstream>
#include <string>

#include "CommonInfoBodyText.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

void CommonInfoBodyText::set_bs_rewritten_text(const CommonInfoBodyText& body_text) {
  bs_pre_text_ = body_text.get_bs_pre_text();

  if (body_text.bs_loop_text()) {
    bs_loop_text_.emplace(*body_text.bs_loop_text());
  }

  if (body_text.bs_post_text()) {
    bs_post_text_.emplace(*body_text.bs_post_text());
  }
}

void CommonInfoBodyText::set_bs_rewritten_text(const std::string &bs_pre_text,
                                               const std::string &bs_loop_text,
                                               const std::string &bs_post_text) {
  bs_pre_text_ = bs_pre_text;

  if (bs_loop_text.size() > 0) {
    bs_loop_text_.emplace(bs_loop_text);
  }

  if (bs_post_text.size() > 0) {
    bs_post_text_.emplace(bs_post_text);
  }
}

const std::string &CommonInfoBodyText::get_bs_loop_text() const {
  if (bs_loop_text_) {
    return *bs_loop_text_;
  }

  static std::string empty;
  return (empty);
}

const std::string &CommonInfoBodyText::get_bs_post_text() const {
  if (bs_post_text_) {
    return *bs_post_text_;
  }

  static std::string empty;
  return (empty);
}

std::string CommonInfoBodyText::bs_body_text() const {
  std::ostringstream oss;

  oss << bs_pre_text_;

  if (bs_loop_text_) {
    oss << *bs_loop_text_;
  }

  if (bs_post_text_) {
    oss << *bs_post_text_;
  }

  return oss.str();
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
