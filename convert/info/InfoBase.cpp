#include "../Env.h"
#include "InfoBase.h"

namespace ks {
namespace ad_algorithm {
namespace convert {

void InfoBase::set_env_ptr(Env* env_ptr) {
  env_ptr_ = env_ptr;
}

Env* InfoBase::env_ptr() const {
  return env_ptr_;
}

Env* InfoBase::mutable_env_ptr() {
  return env_ptr_;
}

Env* InfoBase::parent_env_ptr() const {
  if (env_ptr_ == nullptr) {
    return nullptr;
  }

  return env_ptr_->parent();
}

Env* InfoBase::mutable_parent_env_ptr() {
  if (env_ptr_ == nullptr) {
    return nullptr;
  }

  return env_ptr_->parent();
}

}  // namespace convert
}  // namespace ad_algorithm
}  // namespace ks
