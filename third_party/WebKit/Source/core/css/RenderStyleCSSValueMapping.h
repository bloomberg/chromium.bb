// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RenderStyleCSSValueMapping_h
#define RenderStyleCSSValueMapping_h

#include "core/CSSPropertyNames.h"

namespace blink {

class CSSPrimitiveValue;
class RenderObject;
class RenderStyle;
class ShadowData;
class ShadowList;
class StyleColor;
class Node;

class RenderStyleCSSValueMapping {
public:
    // FIXME: Resolve computed auto alignment in applyProperty/RenderStyle and remove this non-const styledNode parameter.
    static PassRefPtrWillBeRawPtr<CSSValue> get(CSSPropertyID, const RenderStyle&, const RenderObject* renderer = nullptr, Node* styledNode = nullptr, bool allowVisitedStyle = false);
private:
    static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> currentColorOrValidColor(const RenderStyle&, const StyleColor&);
    static PassRefPtrWillBeRawPtr<CSSValue> valueForShadowData(const ShadowData&, const RenderStyle&, bool useSpread);
    static PassRefPtrWillBeRawPtr<CSSValue> valueForShadowList(const ShadowList*, const RenderStyle&, bool useSpread);
    static PassRefPtrWillBeRawPtr<CSSValue> valueForFilter(const RenderStyle&);
};

} // namespace blink

#endif // RenderStyleCSSValueMapping_h
