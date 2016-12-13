// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyDescriptor.h"

#include "core/css/properties/CSSPropertyAPIPadding.h"

namespace blink {

// Initialises a CSSPropertyDescriptor. When functions are added to
// CSSPropertyAPI, also add them to the struct initaliser below.
#define GET_DESCRIPTOR(X) \
  { X::parseSingleValue, true }

// Initialises an invalid CSSPropertyDescriptor. When functions are added to
// CSSPropertyAPI, add a nullptr to represent their function pointers in the
// struct initaliser.
#define GET_INVALID_DESCRIPTOR() \
  { nullptr, false }

static_assert(
    std::is_pod<CSSPropertyDescriptor>::value,
    "CSSPropertyDescriptor must be a POD to support using initializer lists.");

static CSSPropertyDescriptor cssPropertyDescriptors[] = {
    GET_INVALID_DESCRIPTOR(), GET_DESCRIPTOR(CSSPropertyAPIWebkitPadding),
};

const CSSPropertyDescriptor& CSSPropertyDescriptor::get(CSSPropertyID id) {
  // TODO(aazzam): We are currently using hard-coded indexes for
  // cssPropertyDescriptor since we have only implemented a few properties.
  // Later, generate this switch statement, or alternatively return
  // cssPropertyDescriptors[id], and generate the cssPropertyDescriptors array
  // to hold invalid descriptors for methods which haven't been implemented yet.
  switch (id) {
    case CSSPropertyWebkitPaddingEnd:
      return cssPropertyDescriptors[1];
    case CSSPropertyWebkitPaddingStart:
      return cssPropertyDescriptors[1];
    case CSSPropertyWebkitPaddingBefore:
      return cssPropertyDescriptors[1];
    case CSSPropertyWebkitPaddingAfter:
      return cssPropertyDescriptors[1];
    default:
      return cssPropertyDescriptors[0];
  }
}

}  // namespace blink
