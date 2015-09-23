#ifndef VisibilityStyleInterpolation_h
#define VisibilityStyleInterpolation_h

#include "core/CSSValueKeywords.h"
#include "core/CoreExport.h"
#include "core/animation/StyleInterpolation.h"
#include "core/css/CSSPrimitiveValue.h"

namespace blink {

class CORE_EXPORT VisibilityStyleInterpolation : public StyleInterpolation {
public:
    static PassRefPtr<VisibilityStyleInterpolation> create(const CSSValue& start, const CSSValue& end, CSSPropertyID id)
    {
        RefPtr<VisibilityStyleInterpolation> interpolation = adoptRef(new VisibilityStyleInterpolation(
            visibilityToInterpolableValue(start), visibilityToInterpolableValue(end), id));
        if (!isVisible(start)) {
            interpolation->m_notVisible = toCSSPrimitiveValue(start).getValueID();
        } else if (!isVisible(end)) {
            interpolation->m_notVisible = toCSSPrimitiveValue(end).getValueID();
        }

        return interpolation.release();
    }

    static bool canCreateFrom(const CSSValue&);
    static bool isVisible(const CSSValue&);

    void apply(StyleResolverState&) const override;

private:
    VisibilityStyleInterpolation(PassOwnPtr<InterpolableValue> start, PassOwnPtr<InterpolableValue> end, CSSPropertyID id)
        : StyleInterpolation(start, end, id)
        , m_notVisible(CSSValueVisible)
    {
    }

    static PassOwnPtr<InterpolableValue> visibilityToInterpolableValue(const CSSValue&);
    static PassRefPtrWillBeRawPtr<CSSValue> interpolableValueToVisibility(InterpolableValue*, CSSValueID);

    CSSValueID m_notVisible;

    friend class AnimationVisibilityStyleInterpolationTest;
};
}

#endif
