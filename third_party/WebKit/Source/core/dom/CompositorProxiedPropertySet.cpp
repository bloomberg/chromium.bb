// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/CompositorProxiedPropertySet.h"

#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

std::unique_ptr<CompositorProxiedPropertySet>
CompositorProxiedPropertySet::Create() {
  return WTF::WrapUnique(new CompositorProxiedPropertySet);
}

CompositorProxiedPropertySet::CompositorProxiedPropertySet() {
  memset(counts_, 0, sizeof(counts_));
}

CompositorProxiedPropertySet::~CompositorProxiedPropertySet() {}

bool CompositorProxiedPropertySet::IsEmpty() const {
  return !ProxiedProperties();
}

void CompositorProxiedPropertySet::Increment(uint32_t mutable_properties) {
  for (int i = 0; i < CompositorMutableProperty::kNumProperties; ++i) {
    if (mutable_properties & (1 << i))
      ++counts_[i];
  }
}

void CompositorProxiedPropertySet::Decrement(uint32_t mutable_properties) {
  for (int i = 0; i < CompositorMutableProperty::kNumProperties; ++i) {
    if (mutable_properties & (1 << i)) {
      DCHECK(counts_[i]);
      --counts_[i];
    }
  }
}

uint32_t CompositorProxiedPropertySet::ProxiedProperties() const {
  uint32_t properties = CompositorMutableProperty::kNone;
  for (int i = 0; i < CompositorMutableProperty::kNumProperties; ++i) {
    if (counts_[i])
      properties |= 1 << i;
  }
  return properties;
}

}  // namespace blink
