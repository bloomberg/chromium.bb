// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_NEARBY_ATOMIC_BOOLEAN_IMPL_H_
#define CHROMEOS_COMPONENTS_NEARBY_ATOMIC_BOOLEAN_IMPL_H_

#include "base/atomicops.h"
#include "base/macros.h"
#include "chromeos/components/nearby/library/atomic_boolean.h"

namespace chromeos {

namespace nearby {

// Concrete location::nearby::AtomicBoolean implementation.
class AtomicBooleanImpl : public location::nearby::AtomicBoolean {
 public:
  AtomicBooleanImpl();
  ~AtomicBooleanImpl() override;

 private:
  // location::nearby::AtomicBoolean:
  bool get() override;
  void set(bool value) override;

  base::subtle::AtomicWord boolean_ = 0;

  DISALLOW_COPY_AND_ASSIGN(AtomicBooleanImpl);
};

}  // namespace nearby

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_NEARBY_ATOMIC_BOOLEAN_IMPL_H_
