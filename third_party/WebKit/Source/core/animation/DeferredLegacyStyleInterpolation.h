// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DeferredLegacyStyleInterpolation_h
#define DeferredLegacyStyleInterpolation_h

#include "core/CoreExport.h"
#include "core/animation/LegacyStyleInterpolation.h"
#include "core/animation/StyleInterpolation.h"
#include "core/css/CSSValue.h"

namespace blink {

class CSSBasicShapeCircleValue;
class CSSBasicShapeEllipseValue;
class CSSBasicShapePolygonValue;
class CSSBasicShapeInsetValue;
class CSSImageValue;
class CSSPrimitiveValue;
class CSSQuadValue;
class CSSShadowValue;
class CSSSVGDocumentValue;
class CSSValueList;
class CSSValuePair;

class CORE_EXPORT DeferredLegacyStyleInterpolation : public StyleInterpolation {
public:
    static PassRefPtr<DeferredLegacyStyleInterpolation> create(PassRefPtrWillBeRawPtr<CSSValue> start, PassRefPtrWillBeRawPtr<CSSValue> end, CSSPropertyID id)
    {
        return adoptRef(new DeferredLegacyStyleInterpolation(start, end, id));
    }

    void apply(StyleResolverState&) const override;

    static bool interpolationRequiresStyleResolve(const CSSValue&);
    static bool interpolationRequiresStyleResolve(const CSSPrimitiveValue&);
    static bool interpolationRequiresStyleResolve(const CSSImageValue&);
    static bool interpolationRequiresStyleResolve(const CSSShadowValue&);
    static bool interpolationRequiresStyleResolve(const CSSSVGDocumentValue&);
    static bool interpolationRequiresStyleResolve(const CSSValueList&);
    static bool interpolationRequiresStyleResolve(const CSSValuePair&);
    static bool interpolationRequiresStyleResolve(const CSSBasicShapeCircleValue&);
    static bool interpolationRequiresStyleResolve(const CSSBasicShapeEllipseValue&);
    static bool interpolationRequiresStyleResolve(const CSSBasicShapePolygonValue&);
    static bool interpolationRequiresStyleResolve(const CSSBasicShapeInsetValue&);
    static bool interpolationRequiresStyleResolve(const CSSQuadValue&);

private:
    DeferredLegacyStyleInterpolation(PassRefPtrWillBeRawPtr<CSSValue> start, PassRefPtrWillBeRawPtr<CSSValue> end, CSSPropertyID id)
        : StyleInterpolation(InterpolableNumber::create(0), InterpolableNumber::create(1), id)
        , m_startCSSValue(start)
        , m_endCSSValue(end)
        , m_outdated(true)
    {
    }

    RefPtrWillBePersistent<CSSValue> m_startCSSValue;
    RefPtrWillBePersistent<CSSValue> m_endCSSValue;
    mutable RefPtr<LegacyStyleInterpolation> m_innerInterpolation;
    mutable bool m_outdated;
};

} // namespace blink

#endif
