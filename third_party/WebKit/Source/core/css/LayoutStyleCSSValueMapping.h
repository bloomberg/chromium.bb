// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutStyleCSSValueMapping_h
#define LayoutStyleCSSValueMapping_h

#include "core/CSSPropertyNames.h"

namespace blink {

class CSSPrimitiveValue;
class LayoutObject;
class LayoutStyle;
class ShadowData;
class ShadowList;
class StyleColor;
class Node;

class LayoutStyleCSSValueMapping {
public:
    // FIXME: Resolve computed auto alignment in applyProperty/LayoutStyle and remove this non-const styledNode parameter.
    static PassRefPtrWillBeRawPtr<CSSValue> get(CSSPropertyID, const LayoutStyle&, const LayoutObject* renderer = nullptr, Node* styledNode = nullptr, bool allowVisitedStyle = false);
private:
    static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> currentColorOrValidColor(const LayoutStyle&, const StyleColor&);
    static PassRefPtrWillBeRawPtr<CSSValue> valueForShadowData(const ShadowData&, const LayoutStyle&, bool useSpread);
    static PassRefPtrWillBeRawPtr<CSSValue> valueForShadowList(const ShadowList*, const LayoutStyle&, bool useSpread);
    static PassRefPtrWillBeRawPtr<CSSValue> valueForFilter(const LayoutStyle&);
};

} // namespace blink

#endif // LayoutStyleCSSValueMapping_h
