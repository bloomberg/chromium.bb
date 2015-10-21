// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ListInterpolationFunctions_h
#define ListInterpolationFunctions_h

#include "core/animation/InterpolationComponent.h"
#include "wtf/Vector.h"

namespace blink {

class CSSValueList;
class UnderlyingValue;
class InterpolationValue;

class ListInterpolationFunctions {
public:
    template <typename CreateItemCallback>
    static InterpolationComponent createList(size_t length, CreateItemCallback);
    static InterpolationComponent createEmptyList() { return InterpolationComponent(InterpolableList::create(0)); }

    using MergeSingleItemConversionsCallback = PairwiseInterpolationComponent (*)(InterpolationComponent& start, InterpolationComponent& end);
    static PairwiseInterpolationComponent mergeSingleConversions(InterpolationComponent& start, InterpolationComponent& end, MergeSingleItemConversionsCallback);

    using EqualNonInterpolableValuesCallback = bool (*)(const NonInterpolableValue*, const NonInterpolableValue*);
    static bool equalValues(const InterpolationComponent&, const InterpolationComponent&, EqualNonInterpolableValuesCallback);

    using NonInterpolableValuesAreCompatibleCallback = bool (*)(const NonInterpolableValue*, const NonInterpolableValue*);
    using CompositeItemCallback = void (*)(OwnPtr<InterpolableValue>&, RefPtr<NonInterpolableValue>&, double underlyingFraction, const InterpolableValue&, const NonInterpolableValue*);
    static void composite(UnderlyingValue&, double underlyingFraction, const InterpolationValue&, NonInterpolableValuesAreCompatibleCallback, CompositeItemCallback);
};

class NonInterpolableList : public NonInterpolableValue {
public:
    ~NonInterpolableList() final { }

    static PassRefPtr<NonInterpolableList> create(Vector<RefPtr<NonInterpolableValue>>& list)
    {
        return adoptRef(new NonInterpolableList(list));
    }

    size_t length() const { return m_list.size(); }
    const NonInterpolableValue* get(size_t index) const { return m_list[index].get(); }
    NonInterpolableValue* get(size_t index) { return m_list[index].get(); }
    RefPtr<NonInterpolableValue>& getMutable(size_t index) { return m_list[index]; }

    DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

private:
    NonInterpolableList(Vector<RefPtr<NonInterpolableValue>>& list)
    {
        m_list.swap(list);
    }

    Vector<RefPtr<NonInterpolableValue>> m_list;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(NonInterpolableList);

template <typename CreateItemCallback>
InterpolationComponent ListInterpolationFunctions::createList(size_t length, CreateItemCallback createItem)
{
    if (length == 0)
        return createEmptyList();
    OwnPtr<InterpolableList> interpolableList = InterpolableList::create(length);
    Vector<RefPtr<NonInterpolableValue>> nonInterpolableValues(length);
    for (size_t i = 0; i < length; i++) {
        InterpolationComponent component = createItem(i);
        if (!component)
            return nullptr;
        interpolableList->set(i, component.interpolableValue.release());
        nonInterpolableValues[i] = component.nonInterpolableValue.release();
    }
    return InterpolationComponent(interpolableList.release(), NonInterpolableList::create(nonInterpolableValues));
}

} // namespace blink

#endif // ListInterpolationFunctions_h
