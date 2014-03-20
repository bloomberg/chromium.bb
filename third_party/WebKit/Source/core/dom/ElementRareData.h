/*
 * Copyright (C) 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 David Smith <catfish.man@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef ElementRareData_h
#define ElementRareData_h

#include "core/animation/ActiveAnimations.h"
#include "core/dom/DatasetDOMStringMap.h"
#include "core/dom/NamedNodeMap.h"
#include "core/dom/NodeRareData.h"
#include "core/dom/PseudoElement.h"
#include "core/dom/custom/CustomElementDefinition.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/html/ClassList.h"
#include "core/html/ime/InputMethodContext.h"
#include "core/rendering/style/StyleInheritedData.h"
#include "wtf/OwnPtr.h"

namespace WebCore {

class HTMLElement;

class ElementRareData : public NodeRareData {
public:
    static PassOwnPtr<ElementRareData> create(RenderObject* renderer) { return adoptPtr(new ElementRareData(renderer)); }

    ~ElementRareData();

    void setPseudoElement(PseudoId, PassRefPtr<PseudoElement>);
    PseudoElement* pseudoElement(PseudoId) const;

    void resetStyleState();
    void resetDynamicRestyleObservations();

    short tabIndex() const { return m_tabindex; }

    void setTabIndexExplicitly(short index)
    {
        m_tabindex = index;
        setFlag(TabIndexWasSetExplicitly, true);
    }

    void clearTabIndexExplicitly()
    {
        m_tabindex = 0;
        clearFlag(TabIndexWasSetExplicitly);
    }

    bool hasFlag(ElementFlags mask) const { return m_flags & mask; }
    void setFlag(ElementFlags mask, bool value) { m_flags = (m_flags & ~mask) | (-(int32_t)value & mask); }
    void clearFlag(ElementFlags mask) { m_flags &= ~mask; }

    unsigned childIndex() const { return m_childIndex; }
    void setChildIndex(unsigned index) { m_childIndex = index; }

    CSSStyleDeclaration& ensureInlineCSSStyleDeclaration(Element* ownerElement);

    void clearShadow() { m_shadow = nullptr; }
    ElementShadow* shadow() const { return m_shadow.get(); }
    ElementShadow& ensureShadow()
    {
        if (!m_shadow)
            m_shadow = ElementShadow::create();
        return *m_shadow;
    }

    NamedNodeMap* attributeMap() const { return m_attributeMap.get(); }
    void setAttributeMap(PassOwnPtr<NamedNodeMap> attributeMap) { m_attributeMap = attributeMap; }

    RenderStyle* computedStyle() const { return m_computedStyle.get(); }
    void setComputedStyle(PassRefPtr<RenderStyle> computedStyle) { m_computedStyle = computedStyle; }
    void clearComputedStyle() { m_computedStyle = nullptr; }

    ClassList* classList() const { return m_classList.get(); }
    void setClassList(PassOwnPtr<ClassList> classList) { m_classList = classList; }
    void clearClassListValueForQuirksMode()
    {
        if (!m_classList)
            return;
        m_classList->clearValueForQuirksMode();
    }

    DatasetDOMStringMap* dataset() const { return m_dataset.get(); }
    void setDataset(PassOwnPtr<DatasetDOMStringMap> dataset) { m_dataset = dataset; }

    LayoutSize minimumSizeForResizing() const { return m_minimumSizeForResizing; }
    void setMinimumSizeForResizing(LayoutSize size) { m_minimumSizeForResizing = size; }

    IntSize savedLayerScrollOffset() const { return m_savedLayerScrollOffset; }
    void setSavedLayerScrollOffset(IntSize size) { m_savedLayerScrollOffset = size; }

    ActiveAnimations* activeAnimations() { return m_activeAnimations.get(); }
    void setActiveAnimations(PassOwnPtr<ActiveAnimations> activeAnimations)
    {
        m_activeAnimations = activeAnimations;
    }

    bool hasInputMethodContext() const { return m_inputMethodContext; }
    InputMethodContext& ensureInputMethodContext(HTMLElement* element)
    {
        if (!m_inputMethodContext)
            m_inputMethodContext = InputMethodContext::create(element);
        return *m_inputMethodContext;
    }

    bool hasPseudoElements() const;
    void clearPseudoElements();

    void setCustomElementDefinition(PassRefPtr<CustomElementDefinition> definition) { m_customElementDefinition = definition; }
    CustomElementDefinition* customElementDefinition() const { return m_customElementDefinition.get(); }

private:
    short m_tabindex;
    unsigned short m_childIndex;
    unsigned m_flags;

    LayoutSize m_minimumSizeForResizing;
    IntSize m_savedLayerScrollOffset;

    OwnPtr<DatasetDOMStringMap> m_dataset;
    OwnPtr<ClassList> m_classList;
    OwnPtr<ElementShadow> m_shadow;
    OwnPtr<NamedNodeMap> m_attributeMap;
    OwnPtr<InputMethodContext> m_inputMethodContext;
    OwnPtr<ActiveAnimations> m_activeAnimations;
    OwnPtr<InlineCSSStyleDeclaration> m_cssomWrapper;

    RefPtr<RenderStyle> m_computedStyle;
    RefPtr<CustomElementDefinition> m_customElementDefinition;

    RefPtr<PseudoElement> m_generatedBefore;
    RefPtr<PseudoElement> m_generatedAfter;
    RefPtr<PseudoElement> m_backdrop;

    explicit ElementRareData(RenderObject*);
};

inline IntSize defaultMinimumSizeForResizing()
{
    return IntSize(LayoutUnit::max(), LayoutUnit::max());
}

inline ElementRareData::ElementRareData(RenderObject* renderer)
    : NodeRareData(renderer)
    , m_tabindex(0)
    , m_childIndex(0)
    , m_flags(0)
    , m_minimumSizeForResizing(defaultMinimumSizeForResizing())
{
}

inline ElementRareData::~ElementRareData()
{
    ASSERT(!m_shadow);
    ASSERT(!m_generatedBefore);
    ASSERT(!m_generatedAfter);
    ASSERT(!m_backdrop);
}

inline bool ElementRareData::hasPseudoElements() const
{
    return m_generatedBefore || m_generatedAfter || m_backdrop;
}

inline void ElementRareData::clearPseudoElements()
{
    setPseudoElement(BEFORE, nullptr);
    setPseudoElement(AFTER, nullptr);
    setPseudoElement(BACKDROP, nullptr);
}

inline void ElementRareData::setPseudoElement(PseudoId pseudoId, PassRefPtr<PseudoElement> element)
{
    switch (pseudoId) {
    case BEFORE:
        if (m_generatedBefore)
            m_generatedBefore->dispose();
        m_generatedBefore = element;
        break;
    case AFTER:
        if (m_generatedAfter)
            m_generatedAfter->dispose();
        m_generatedAfter = element;
        break;
    case BACKDROP:
        if (m_backdrop)
            m_backdrop->dispose();
        m_backdrop = element;
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

inline PseudoElement* ElementRareData::pseudoElement(PseudoId pseudoId) const
{
    switch (pseudoId) {
    case BEFORE:
        return m_generatedBefore.get();
    case AFTER:
        return m_generatedAfter.get();
    case BACKDROP:
        return m_backdrop.get();
    default:
        return 0;
    }
}

inline void ElementRareData::resetStyleState()
{
    clearFlag(StyleAffectedByEmpty);
    setChildIndex(0);
}

inline void ElementRareData::resetDynamicRestyleObservations()
{
    clearFlag(ChildrenAffectedByFocus);
    clearFlag(ChildrenAffectedByHover);
    clearFlag(ChildrenAffectedByActive);
    clearFlag(ChildrenAffectedByDrag);
    clearFlag(ChildrenAffectedByFirstChildRules);
    clearFlag(ChildrenAffectedByLastChildRules);
    clearFlag(ChildrenAffectedByDirectAdjacentRules);
    clearFlag(ChildrenAffectedByIndirectAdjacentRules);
    clearFlag(ChildrenAffectedByForwardPositionalRules);
    clearFlag(ChildrenAffectedByBackwardPositionalRules);
}

} // namespace

#endif // ElementRareData_h
