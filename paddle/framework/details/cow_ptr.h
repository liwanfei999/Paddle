/* Copyright (c) 2016 PaddlePaddle Authors. All Rights Reserved.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License. */

#pragma once
#include <memory>
#include <thread>

namespace paddle {
namespace framework {
namespace details {

// Change it to thread safe flags if needed.
class ThreadUnsafeOwnershipFlags {
 public:
  ThreadUnsafeOwnershipFlags(bool flag) : flag_(flag) {}

  ThreadUnsafeOwnershipFlags(const ThreadUnsafeOwnershipFlags& o) = delete;
  ThreadUnsafeOwnershipFlags& operator=(const ThreadUnsafeOwnershipFlags& o) =
      delete;
  ThreadUnsafeOwnershipFlags(ThreadUnsafeOwnershipFlags&& o) = default;

  void SetOwnership(bool flag) { flag_ = flag; }

  template <typename Callback>
  void AcquireOwnershipOnce(Callback acquire) {
    if (!flag_) {
      acquire();
      flag_ = true;
    }
  }

 private:
  bool flag_;
};

// Copy On Write pointer.
// It will hold a T* pointer, and only copy once when `MutableData` is invoked.
//
// The template parameter OwnershipFlags should have:
//   * a constructor takes a bool. True if own.
//   * SetOwnership(bool flag).
//   * AcquireOwnershipOnce(Callback). It will invoke the callback if it is not
//     owned.
template <typename T, typename OwnershipFlags = ThreadUnsafeOwnershipFlags>
class COWPtr {
 public:
  // Ctor from raw pointer.
  explicit COWPtr(T* ptr) : payload_(ptr), ownership_{true} {}

  // Move methods. Steal ownership from origin
  COWPtr(COWPtr&& o)
      : payload_(o.payload_), ownership_{std::move(o.ownership_)} {}
  COWPtr& operator=(COWPtr&& origin) = default;

  // Copy methods. Not own payload
  COWPtr(const COWPtr& o) : payload_(o.payload_), ownership_{false} {}
  COWPtr& operator=(const COWPtr& o) {
    payload_ = o.payload_;
    ownership_.SetOwnership(false);
    return *this;
  }

  const T& Data() const { return *payload_; }

  T* MutableData() {
    ownership_.AcquireOwnershipOnce(
        [this] { payload_.reset(new T(*payload_)); });
    return payload_.get();
  }

  void Reset() {
    ownership_.AcquireOwnershipOnce([this] { payload_.reset(); });
    payload_.reset(new T());
  }

 private:
  std::shared_ptr<T> payload_;
  OwnershipFlags ownership_;
};

}  // namespace details
}  // namespace framework
}  // namespace paddle
