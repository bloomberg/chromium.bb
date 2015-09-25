// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutEditor_h
#define LayoutEditor_h

#include "core/CSSPropertyNames.h"
#include "core/CoreExport.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSRuleList.h"
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
class ScriptController;

class CORE_EXPORT LayoutEditor final : public NoBaseWillBeGarbageCollectedFinalized<LayoutEditor> {
    WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED(LayoutEditor);
public:
    static PassOwnPtrWillBeRawPtr<LayoutEditor> create(Element* element, InspectorCSSAgent* cssAgent, InspectorDOMAgent* domAgent, ScriptController* scriptController)
    {
        return adoptPtrWillBeNoop(new LayoutEditor(element, cssAgent, domAgent, scriptController));
    }

    ~LayoutEditor();
    void dispose();

    Element* element() { return m_element.get(); }
    void overlayStartedPropertyChange(const String&);
    void overlayPropertyChanged(float);
    void overlayEndedPropertyChange();
    void commitChanges();
    void nextSelector();
    void previousSelector();
    String currentSelectorInfo();
    void rebuild() const;
    DECLARE_TRACE();

private:
    LayoutEditor(Element*, InspectorCSSAgent*, InspectorDOMAgent*, ScriptController*);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> getPropertyCSSValue(CSSPropertyID) const;
    PassRefPtr<JSONObject> createValueDescription(const String&) const;
    void appendAnchorFor(JSONArray*, const String&, const String&, const FloatPoint&, const FloatPoint&) const;
    bool setCSSPropertyValueInCurrentRule(const String&);
    bool currentStyleIsInline();
    void evaluateInOverlay(const String&, PassRefPtr<JSONValue>) const;

    RefPtrWillBeMember<Element> m_element;
    RawPtrWillBeMember<InspectorCSSAgent> m_cssAgent;
    RawPtrWillBeMember<InspectorDOMAgent> m_domAgent;
    RawPtrWillBeMember<ScriptController> m_scriptController;
    CSSPropertyID m_changingProperty;
    float m_propertyInitialValue;
    float m_factor;
    CSSPrimitiveValue::UnitType m_valueUnitType;
    bool m_isDirty;

    RefPtrWillBeMember<CSSRuleList> m_matchedRules;
    // When m_currentRuleIndex == m_matchedRules.length(), current style is inline style.
    unsigned m_currentRuleIndex;
};

} // namespace blink


#endif // !defined(LayoutEditor_h)
