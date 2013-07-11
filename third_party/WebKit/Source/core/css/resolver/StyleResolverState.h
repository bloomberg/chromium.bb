/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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

#ifndef StyleResolverState_h
#define StyleResolverState_h

#include "CSSPropertyNames.h"

#include "core/css/CSSSVGDocumentValue.h"
#include "core/css/CSSToStyleMap.h"
#include "core/css/resolver/ElementStyleResources.h"
#include "core/dom/Element.h"
#include "core/platform/graphics/Color.h"
#include "core/rendering/style/CachedUAStyle.h"
#include "core/rendering/style/RenderStyle.h"
#include "core/rendering/style/StyleInheritedData.h"
#include "wtf/HashMap.h"

namespace WebCore {

class FontDescription;
class RenderRegion;

class StyleResolverState {
WTF_MAKE_NONCOPYABLE(StyleResolverState);
public:
    StyleResolverState()
    : m_element(0)
    , m_childIndex(0)
    , m_styledElement(0)
    , m_parentNode(0)
    , m_parentStyle(0)
    , m_rootElementStyle(0)
    , m_regionForStyling(0)
    , m_elementLinkState(NotInsideLink)
    , m_distributedToInsertionPoint(false)
    , m_elementAffectedByClassRules(false)
    , m_applyPropertyToRegularStyle(true)
    , m_applyPropertyToVisitedLinkStyle(false)
    , m_lineHeightValue(0)
    , m_fontDirty(false)
    , m_styleMap(*this, m_elementStyleResources)
    { }

    void initForStyleResolve(Document*, Element*, int childIndex = 0, RenderStyle* parentStyle = 0, RenderRegion* regionForStyling = 0);
    void clear();

    Document* document() const { return m_element->document(); }
    Element* element() const { return m_element; }
    int childIndex() const { return m_childIndex; }
    Element* styledElement() const { return m_styledElement; }
    void setStyle(PassRefPtr<RenderStyle> style) { m_style = style; }
    RenderStyle* style() const { return m_style.get(); }
    PassRefPtr<RenderStyle> takeStyle() { return m_style.release(); }

    const ContainerNode* parentNode() const { return m_parentNode; }
    void setParentStyle(PassRefPtr<RenderStyle> parentStyle) { m_parentStyle = parentStyle; }
    RenderStyle* parentStyle() const { return m_parentStyle.get(); }
    RenderStyle* rootElementStyle() const { return m_rootElementStyle; }

    const RenderRegion* regionForStyling() const { return m_regionForStyling; }
    EInsideLink elementLinkState() const { return m_elementLinkState; }
    bool distributedToInsertionPoint() const { return m_distributedToInsertionPoint; }
    void setElementAffectedByClassRules(bool isAffected) { m_elementAffectedByClassRules = isAffected; }
    bool elementAffectedByClassRules() const { return m_elementAffectedByClassRules; }

    // FIXME: These are effectively side-channel "out parameters" for the various
    // map functions. When we map from CSS to style objects we use this state object
    // to track various meta-data about that mapping (e.g. if it's cache-able).
    // We need to move this data off of StyleResolverState and closer to the
    // objects it applies to. Possibly separating (immutable) inputs from (mutable) outputs.
    void setApplyPropertyToRegularStyle(bool isApply) { m_applyPropertyToRegularStyle = isApply; }
    void setApplyPropertyToVisitedLinkStyle(bool isApply) { m_applyPropertyToVisitedLinkStyle = isApply; }
    bool applyPropertyToRegularStyle() const { return m_applyPropertyToRegularStyle; }
    bool applyPropertyToVisitedLinkStyle() const { return m_applyPropertyToVisitedLinkStyle; }

    void setLineHeightValue(CSSValue* value) { m_lineHeightValue = value; }
    CSSValue* lineHeightValue() { return m_lineHeightValue; }

    void cacheUserAgentBorderAndBackground() { m_cachedUAStyle = CachedUAStyle(style()); }
    const CachedUAStyle& cachedUAStyle() const
    {
        // We should only ever be asking for these cached values
        // if the style being resolved has appearance.
        ASSERT(style()->hasAppearance());
        return m_cachedUAStyle;
    }

    ElementStyleResources& elementStyleResources() { return m_elementStyleResources; }
    const CSSToStyleMap& styleMap() const { return m_styleMap; }
    CSSToStyleMap& styleMap() { return m_styleMap; }

    // FIXME: Once styleImage can be made to not take a StyleResolverState
    // this convenience function should be removed. As-is, without this, call
    // sites are extremely verbose.
    PassRefPtr<StyleImage> styleImage(CSSPropertyID propertyId, CSSValue* value)
    {
        return m_elementStyleResources.styleImage(document()->textLinkColors(), style()->visitedDependentColor(CSSPropertyColor), propertyId, value);
    }

    // FIXME: These exist as a primitive way to track mutations to font-related properties
    // on a RenderStyle. As designed, these are very error-prone, as some callers
    // set these directly on the RenderStyle w/o telling us. Presumably we'll
    // want to design a better wrapper around RenderStyle for tracking these mutations
    // and separate it from StyleResolverState.
    const FontDescription& fontDescription() { return m_style->fontDescription(); }
    const FontDescription& parentFontDescription() { return m_parentStyle->fontDescription(); }
    void setFontDescription(const FontDescription& fontDescription) { m_fontDirty |= m_style->setFontDescription(fontDescription); }
    void setZoom(float f) { m_fontDirty |= m_style->setZoom(f); }
    void setEffectiveZoom(float f) { m_fontDirty |= m_style->setEffectiveZoom(f); }
    void setWritingMode(WritingMode writingMode) { m_fontDirty |= m_style->setWritingMode(writingMode); }
    void setTextOrientation(TextOrientation textOrientation) { m_fontDirty |= m_style->setTextOrientation(textOrientation); }
    void setFontDirty(bool isDirty) { m_fontDirty = isDirty; }
    bool fontDirty() const { return m_fontDirty; }

    // SVG handles zooming in a different way compared to CSS. The whole document is scaled instead
    // of each individual length value in the render style / tree. CSSPrimitiveValue::computeLength*()
    // multiplies each resolved length with the zoom multiplier - so for SVG we need to disable that.
    // Though all CSS values that can be applied to outermost <svg> elements (width/height/border/padding...)
    // need to respect the scaling. RenderBox (the parent class of RenderSVGRoot) grabs values like
    // width/height/border/padding/... from the RenderStyle -> for SVG these values would never scale,
    // if we'd pass a 1.0 zoom factor everyhwere. So we only pass a zoom factor of 1.0 for specific
    // properties that are NOT allowed to scale within a zoomed SVG document (letter/word-spacing/font-size).
    bool useSVGZoomRules() const { return m_element && m_element->isSVGElement(); }

private:
    void initElement(Element*, int childIndex);

    Element* m_element;
    int m_childIndex;
    RefPtr<RenderStyle> m_style;
    Element* m_styledElement;
    ContainerNode* m_parentNode;
    RefPtr<RenderStyle> m_parentStyle;
    RenderStyle* m_rootElementStyle;

    // Required to ASSERT in applyProperties.
    RenderRegion* m_regionForStyling;

    EInsideLink m_elementLinkState;

    bool m_distributedToInsertionPoint;

    bool m_elementAffectedByClassRules;

    bool m_applyPropertyToRegularStyle;
    bool m_applyPropertyToVisitedLinkStyle;

    CSSValue* m_lineHeightValue;
    bool m_fontDirty;

    CachedUAStyle m_cachedUAStyle;
    ElementStyleResources m_elementStyleResources;
    // CSSToStyleMap is a pure-logic class and only contains
    // a back-pointer to this object.
    CSSToStyleMap m_styleMap;
};

} // namespace WebCore

#endif // StyleResolverState_h
