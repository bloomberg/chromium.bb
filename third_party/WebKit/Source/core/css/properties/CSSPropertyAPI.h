// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyAPI_h
#define CSSPropertyAPI_h

#include "core/CSSPropertyNames.h"
#include "core/css/CSSValue.h"
#include "core/css/parser/CSSParserMode.h"
#include "core/css/parser/CSSParserTokenRange.h"

namespace blink {

// We will use this API to represent all functions used for property-specific
// logic inside the blink style engine. All specific properties are subclasses
// of CSSPropertyAPI.
//
// To add a new property using this API:
// - Make a class that implements CSSPropertyAPI, and implement the static
//   methods.
// - Update the cssPropertyDescriptors array in CSSPropertyDescriptor.cpp to
//   call GET_DESCRIPTOR(classname).
//
// To add new functions using this API:
// - New functions and static variables can be added in this class. A default
//   implementation of functions can optionally be provided.
// - When adding new functions, also add them to GET_DESCRIPTOR, and the get()
//   method in CSSPropertyDescriptors.cpp, and the descriptor struct in
//   CSSPropertyDescriptor.h.
class CSSPropertyAPI {
  STATIC_ONLY(CSSPropertyAPI);

 public:
  // Parses a single CSS property and returns the corresponding CSSValue. If the
  // input is invalid it returns nullptr.
  static const CSSValue* parseSingleValue(CSSParserTokenRange&,
                                          const CSSParserContext&);
};

}  // namespace blink

#endif  // CSSPropertyAPI_h
