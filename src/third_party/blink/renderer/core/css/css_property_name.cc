// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_property_name.h"

#include "third_party/blink/renderer/core/css/properties/css_property.h"

namespace blink {

namespace {

// TODO(andruud): Reduce this to sizeof(void*).
struct SameSizeAsCSSPropertyName {
  CSSPropertyID property_id_;
  AtomicString custom_property_name_;
};

static_assert(sizeof(CSSPropertyName) == sizeof(SameSizeAsCSSPropertyName),
              "CSSPropertyName should stay small");

}  // namespace

AtomicString CSSPropertyName::ToAtomicString() const {
  if (IsCustomProperty())
    return custom_property_name_;
  return CSSProperty::Get(property_id_).GetPropertyNameAtomicString();
}

}  // namespace blink
