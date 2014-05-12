// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSTransitionData_h
#define CSSTransitionData_h

#include "CSSPropertyNames.h"
#include "core/animation/css/CSSTimingData.h"
#include "wtf/Vector.h"

namespace WebCore {

class CSSTransitionData FINAL : public CSSTimingData {
public:
    enum TransitionPropertyType {
        TransitionNone,
        TransitionSingleProperty,
        TransitionAll
    };

    // FIXME: This is incorrect as per CSS3 Transitions, we should allow the
    // user to specify unknown properties and keep them in the list (crbug.com/304020).
    // Also, we shouldn't allow 'none' to be used alongside other properties.
    struct TransitionProperty {
        TransitionProperty(CSSPropertyID id)
            : propertyType(TransitionSingleProperty)
            , propertyId(id)
        {
            ASSERT(id != CSSPropertyInvalid);
        }

        TransitionProperty(TransitionPropertyType type)
            : propertyType(type)
            , propertyId(CSSPropertyInvalid)
        {
            ASSERT(type != TransitionSingleProperty);
        }

        bool operator==(const TransitionProperty& other) const { return propertyType == other.propertyType && propertyId == other.propertyId; }

        TransitionPropertyType propertyType;
        CSSPropertyID propertyId;
    };

    static PassOwnPtrWillBeRawPtr<CSSTransitionData> create()
    {
        return adoptPtrWillBeNoop(new CSSTransitionData);
    }

    static PassOwnPtrWillBeRawPtr<CSSTransitionData> create(const CSSTransitionData& transitionData)
    {
        return adoptPtrWillBeNoop(new CSSTransitionData(transitionData));
    }

    bool transitionsMatchForStyleRecalc(const CSSTransitionData& other) const;

    Timing convertToTiming(size_t index) const;

    const Vector<TransitionProperty>& propertyList() const { return m_propertyList; }
    Vector<TransitionProperty>& propertyList() { return m_propertyList; }

    static TransitionProperty initialProperty() { return TransitionProperty(TransitionAll); }

private:
    CSSTransitionData();
    explicit CSSTransitionData(const CSSTransitionData&);

    Vector<TransitionProperty> m_propertyList;
};

} // namespace WebCore

#endif // CSSTransitionData_h
