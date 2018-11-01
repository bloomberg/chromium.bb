// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_initial_data.h"

#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/style/data_equivalency.h"

namespace blink {

StyleInitialData::StyleInitialData(const PropertyRegistry& registry)
    : variables_(MakeGarbageCollected<
                 HeapHashMap<AtomicString, Member<const CSSValue>>>()) {
  for (const auto& entry : registry) {
    const CSSValue* initial = entry.value->Initial();
    if (!initial)
      continue;
    variables_->Set(entry.key, initial);
  }
}

StyleInitialData::StyleInitialData(StyleInitialData& other)
    : variables_(MakeGarbageCollected<
                 HeapHashMap<AtomicString, Member<const CSSValue>>>(
          *other.variables_)) {}

bool StyleInitialData::operator==(const StyleInitialData& other) const {
  if (variables_->size() != other.variables_->size())
    return false;

  for (const auto& iter : *variables_) {
    const CSSValue* other_value = other.variables_->at(iter.key);
    if (iter.value != other_value)
      return false;
  }

  return true;
}

}  // namespace blink
