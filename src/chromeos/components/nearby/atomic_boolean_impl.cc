// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/nearby/atomic_boolean_impl.h"

namespace {

base::subtle::AtomicWord BoolToAtomicWord(bool value) {
  return value ? 1 : 0;
}

bool AtomicWordToBool(base::subtle::AtomicWord value) {
  return value == 0 ? false : true;
}

}  // namespace

namespace chromeos {

namespace nearby {

AtomicBooleanImpl::AtomicBooleanImpl() = default;

AtomicBooleanImpl::~AtomicBooleanImpl() = default;

bool AtomicBooleanImpl::get() {
  return AtomicWordToBool(base::subtle::Acquire_Load(&boolean_));
}

void AtomicBooleanImpl::set(bool value) {
  base::subtle::Release_Store(&boolean_, BoolToAtomicWord(value));
}

}  // namespace nearby

}  // namespace chromeos
