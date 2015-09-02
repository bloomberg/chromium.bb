// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutEditor_h
#define LayoutEditor_h

#include "core/CSSPropertyNames.h"
#include "core/CoreExport.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/dom/Element.h"
#include "platform/heap/Handle.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class CSSPrimitiveValue;
class InspectorCSSAgent;
class InspectorDOMAgent;
class JSONArray;
class JSONObject;

class CORE_EXPORT LayoutEditor final : public NoBaseWillBeGarbageCollectedFinalized<LayoutEditor> {
public:
    static PassOwnPtrWillBeRawPtr<LayoutEditor> create(InspectorCSSAgent* cssAgent, InspectorDOMAgent* domAgent)
    {
        return adoptPtrWillBeNoop(new LayoutEditor(cssAgent, domAgent));
    }

    void selectNode(Node*);
    Node* node() { return m_element.get(); }
    PassRefPtr<JSONObject> buildJSONInfo() const;
    void overlayStartedPropertyChange(const String&);
    void overlayPropertyChanged(float);
    void overlayEndedPropertyChange();
    void clearSelection(bool);

    DECLARE_TRACE();

private:
    LayoutEditor(InspectorCSSAgent*, InspectorDOMAgent*);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> getPropertyCSSValue(CSSPropertyID) const;
    PassRefPtr<JSONObject> createValueDescription(const String&) const;
    void appendAnchorFor(JSONArray*, const String&, const String&, const FloatPoint&, const FloatPoint&) const;

    RefPtrWillBeMember<Element> m_element;
    RawPtrWillBeMember<InspectorCSSAgent> m_cssAgent;
    RawPtrWillBeMember<InspectorDOMAgent> m_domAgent;
    CSSPropertyID m_changingProperty;
    float m_propertyInitialValue;
    float m_factor;
    CSSPrimitiveValue::UnitType m_valueUnitType;
    bool m_isDirty;
};

} // namespace blink


#endif // !defined(LayoutEditor_h)
