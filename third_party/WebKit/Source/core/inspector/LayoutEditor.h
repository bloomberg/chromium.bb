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
    USING_FAST_MALLOC_WILL_BE_REMOVED(LayoutEditor);
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
    void rebuild();
    DECLARE_TRACE();

private:
    LayoutEditor(Element*, InspectorCSSAgent*, InspectorDOMAgent*, ScriptController*);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> getPropertyCSSValue(CSSPropertyID) const;
    PassRefPtr<JSONObject> createValueDescription(const String&);
    void appendAnchorFor(JSONArray*, const String&, const String&);
    bool setCSSPropertyValueInCurrentRule(const String&);
    void editableSelectorUpdated(bool hasChanged) const;
    void evaluateInOverlay(const String&, PassRefPtr<JSONValue>) const;
    PassRefPtr<JSONObject> currentSelectorInfo(CSSStyleDeclaration*) const;
    bool growInside(String propertyName, CSSPrimitiveValue*);

    RefPtrWillBeMember<Element> m_element;
    RawPtrWillBeMember<InspectorCSSAgent> m_cssAgent;
    RawPtrWillBeMember<InspectorDOMAgent> m_domAgent;
    RawPtrWillBeMember<ScriptController> m_scriptController;
    CSSPropertyID m_changingProperty;
    float m_propertyInitialValue;
    float m_factor;
    CSSPrimitiveValue::UnitType m_valueUnitType;
    bool m_isDirty;

    WillBeHeapVector<RefPtrWillBeMember<CSSStyleDeclaration>> m_matchedStyles;
    HashMap<String, bool> m_growsInside;
    unsigned m_currentRuleIndex;
};

} // namespace blink


#endif // !defined(LayoutEditor_h)
