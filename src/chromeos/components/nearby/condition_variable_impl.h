// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_NEARBY_CONDITION_VARIABLE_IMPL_H_
#define CHROMEOS_COMPONENTS_NEARBY_CONDITION_VARIABLE_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/synchronization/waitable_event.h"
#include "chromeos/components/nearby/library/condition_variable.h"
#include "chromeos/components/nearby/library/exception.h"
#include "chromeos/components/nearby/library/lock.h"

namespace chromeos {

namespace nearby {

// Concrete location::nearby::ConditionVariable implementation.
class ConditionVariableImpl : public location::nearby::ConditionVariable {
 public:
  explicit ConditionVariableImpl(location::nearby::Lock* lock);
  ~ConditionVariableImpl() override;

 private:
  // location::nearby::ConditionVariable:
  void notify() override;

  // Expects the calling thread to already own |lock_|. Calling this without
  // ownership of |lock_| results in undefined behavior, or at least behavior
  // specified by the specific implementation of |lock_|.
  location::nearby::Exception::Value wait() override;

  location::nearby::Lock* const lock_;
  base::WaitableEvent notify_has_been_called_;

  DISALLOW_COPY_AND_ASSIGN(ConditionVariableImpl);
};

}  // namespace nearby

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_NEARBY_CONDITION_VARIABLE_IMPL_H_
