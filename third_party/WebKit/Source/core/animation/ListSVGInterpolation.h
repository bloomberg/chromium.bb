// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ListSVGInterpolation_h
#define ListSVGInterpolation_h

#include "core/animation/SVGInterpolation.h"
#include "core/svg/properties/SVGAnimatedProperty.h"
#include "core/svg/properties/SVGProperty.h"

namespace blink {

template<typename InterpolationType, typename NonInterpolableType>
class ListSVGInterpolationImpl;

// TODO(ericwilligers): Implement ListSVGInterpolationImpl for non-void NonInterpolableType

template<typename InterpolationType>
class ListSVGInterpolationImpl<InterpolationType, void> : public SVGInterpolation {
public:
    typedef typename InterpolationType::ListType ListType;

    static PassRefPtrWillBeRawPtr<ListSVGInterpolationImpl<InterpolationType, void>> maybeCreate(SVGPropertyBase* start, SVGPropertyBase* end, PassRefPtrWillBeRawPtr<SVGAnimatedPropertyBase> attribute)
    {
        ASSERT(start->type() == ListType::classType());
        ASSERT(end->type() == ListType::classType());

        ListType* startList = static_cast<ListType*>(start);
        ListType* endList = static_cast<ListType*>(end);
        if (startList->length() != endList->length())
            return nullptr;

        return adoptRefWillBeNoop(new ListSVGInterpolationImpl<InterpolationType, void>(toInterpolableValue(startList), toInterpolableValue(endList), attribute));
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        SVGInterpolation::trace(visitor);
    }

private:
    ListSVGInterpolationImpl(PassOwnPtrWillBeRawPtr<InterpolableValue> start, PassOwnPtrWillBeRawPtr<InterpolableValue> end, PassRefPtrWillBeRawPtr<SVGAnimatedPropertyBase> attribute)
        : SVGInterpolation(start, end, attribute)
    {
    }

    static PassOwnPtrWillBeRawPtr<InterpolableValue> toInterpolableValue(ListType* listValue)
    {
        size_t length = listValue->length();
        OwnPtrWillBeRawPtr<InterpolableList> result = InterpolableList::create(length);
        for (size_t i = 0; i < length; i++)
            result->set(i, InterpolationType::toInterpolableValue(listValue->at(i)));
        return result.release();
    }

    static PassRefPtrWillBeRawPtr<ListType> fromInterpolableValue(const InterpolableValue& value)
    {
        const InterpolableList& listValue = toInterpolableList(value);
        RefPtrWillBeRawPtr<ListType> result = ListType::create();
        for (size_t i = 0; i < listValue.length(); i++) {
            result->append(InterpolationType::fromInterpolableValue(*listValue.get(i)));
        }
        return result.release();
    }

    virtual PassRefPtrWillBeRawPtr<SVGPropertyBase> interpolatedValue(SVGElement& element) const
    {
        return fromInterpolableValue(*m_cachedValue);
    }
};

template<typename InterpolationType>
class ListSVGInterpolation {
public:
    static PassRefPtrWillBeRawPtr<ListSVGInterpolationImpl<InterpolationType, typename InterpolationType::NonInterpolableType>>  maybeCreate(SVGPropertyBase* start, SVGPropertyBase* end, PassRefPtrWillBeRawPtr<SVGAnimatedPropertyBase> attribute)
    {
        return ListSVGInterpolationImpl<InterpolationType, typename InterpolationType::NonInterpolableType>::maybeCreate(start, end, attribute);
    }
};

} // namespace blink

#endif // ListSVGInterpolation_h
