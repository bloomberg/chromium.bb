// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyAPI_h
#define CSSPropertyAPI_h

#include "core/CSSPropertyNames.h"
#include "wtf/Allocator.h"

namespace blink {

class CSSValue;
class CSSParserContext;
class CSSParserTokenRange;

// We will use this API to represent all functions used for property-specific
// logic inside the blink style engine. All specific properties are subclasses
// of CSSPropertyAPI.
//
// To add a new implementation of this API for a property:
// - Make a class that implements CSSPropertyAPI.
// - For each method that you wish to implement in this class, add this method
//   name to the api_methods flag in CSSProperties.json5.
// - Implement these methods in the .cpp file.
//
// To add new functions to this API:
// - Add the function to the struct below.
// - Add the function to CSSPropertyAPIFiles.h.tmpl template file, surrounded
//   with an {% if "methodName" in implementedMethods %} check.
// - Add the function name to the valid_methods array in
//   make_css_property_apis.py.
// - Add function to the initializer list in the cssPropertyDescriptors array
//   in CSSPropertyDescriptor.cpp.tmpl, and add a nullptr to the invalid
//   CSSPropertyDescriptor.
class CSSPropertyAPI {
  STATIC_ONLY(CSSPropertyAPI);

 public:
  // Parses a single CSS property and returns the corresponding CSSValue. If the
  // input is invalid it returns nullptr.
  static const CSSValue* parseSingleValue(CSSParserTokenRange&,
                                          const CSSParserContext*) {
    // No code should reach here, since properties either have their own
    // implementations of this method or store nullptr in their descriptor.
    NOTREACHED();
    return nullptr;
  }
};

}  // namespace blink

#endif  // CSSPropertyAPI_h
