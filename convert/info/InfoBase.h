#pragma once

#include <map>
#include <unordered_map>

namespace ks {
namespace ad_algorithm {
namespace convert {

class Env;

class InfoBase {
 public:
  InfoBase() = default;

  void set_env_ptr(Env* env_ptr);

  Env* env_ptr() const;
  Env* mutable_env_ptr();

  Env* parent_env_ptr() const;
  Env* mutable_parent_env_ptr();

 protected:
  Env* env_ptr_ = nullptr;
};

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
