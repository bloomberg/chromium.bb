// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_NEARBY_LOCK_BASE_H_
#define CHROMEOS_COMPONENTS_NEARBY_LOCK_BASE_H_

#include "base/macros.h"
#include "chromeos/components/nearby/library/lock.h"

namespace chromeos {

namespace nearby {

class LockBase : public location::nearby::Lock {
 private:
  friend class ConditionVariableImpl;

  // Returns whether the invoking thread is the same one that currently holds
  // this Lock.
  virtual bool IsHeldByCurrentThread() = 0;
};

}  // namespace nearby

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_NEARBY_LOCK_BASE_H_
