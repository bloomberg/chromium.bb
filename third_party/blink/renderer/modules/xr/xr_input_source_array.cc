// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_input_source_array.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"

namespace blink {

XRInputSource* XRInputSourceArray::AnonymousIndexedGetter(
    unsigned index) const {
  if (index >= input_sources_.size())
    return nullptr;

  return input_sources_[index];
}

void XRInputSourceArray::Add(XRInputSource* input_source) {
  input_sources_.push_back(input_source);
}

void XRInputSourceArray::Trace(blink::Visitor* visitor) {
  visitor->Trace(input_sources_);
  ScriptWrappable::Trace(visitor);
}
}  // namespace blink
