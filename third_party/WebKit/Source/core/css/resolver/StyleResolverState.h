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

#include "core/animation/css/CSSAnimations.h"
#include "core/css/CSSSVGDocumentValue.h"
#include "core/css/CSSToStyleMap.h"
#include "core/css/resolver/ElementResolveContext.h"
#include "core/css/resolver/ElementStyleResources.h"
#include "core/css/resolver/FontBuilder.h"
#include "core/dom/Element.h"
#include "core/rendering/style/CachedUAStyle.h"
#include "core/rendering/style/RenderStyle.h"
#include "core/rendering/style/StyleInheritedData.h"

namespace WebCore {

class FontDescription;
class RenderRegion;
class StyleRule;

class StyleResolverState {
WTF_MAKE_NONCOPYABLE(StyleResolverState);
public:
    StyleResolverState(Document&, Element*, RenderStyle* parentStyle = 0, RenderRegion* regionForStyling = 0);
    ~StyleResolverState();

    // In FontFaceSet and CanvasRenderingContext2D, we don't have an element to grab the document from.
    // This is why we have to store the document separately.
    Document& document() const { return m_document; }
    // These are all just pass-through methods to ElementResolveContext.
    Element* element() const { return m_elementContext.element(); }
    const ContainerNode* parentNode() const { return m_elementContext.parentNode(); }
    const RenderStyle* rootElementStyle() const { return m_elementContext.rootElementStyle(); }
    EInsideLink elementLinkState() const { return m_elementContext.elementLinkState(); }
    bool distributedToInsertionPoint() const { return m_elementContext.distributedToInsertionPoint(); }

    const ElementResolveContext& elementContext() const { return m_elementContext; }

    void setStyle(PassRefPtr<RenderStyle> style) { m_style = style; }
    const RenderStyle* style() const { return m_style.get(); }
    RenderStyle* style() { return m_style.get(); }
    PassRefPtr<RenderStyle> takeStyle() { return m_style.release(); }

    void setAnimationUpdate(PassOwnPtr<CSSAnimationUpdate> update) { m_animationUpdate = update; }
    const CSSAnimationUpdate* animationUpdate() { return m_animationUpdate.get(); }
    PassOwnPtr<CSSAnimationUpdate> takeAnimationUpdate() { return m_animationUpdate.release(); }

    void setParentStyle(PassRefPtr<RenderStyle> parentStyle) { m_parentStyle = parentStyle; }
    const RenderStyle* parentStyle() const { return m_parentStyle.get(); }
    RenderStyle* parentStyle() { return m_parentStyle.get(); }

    const RenderRegion* regionForStyling() const { return m_regionForStyling; }

    void setCurrentRule(StyleRule* currentRule) { m_currentRule = currentRule; }
    const StyleRule* currentRule() const { return m_currentRule; }

    // FIXME: These are effectively side-channel "out parameters" for the various
    // map functions. When we map from CSS to style objects we use this state object
    // to track various meta-data about that mapping (e.g. if it's cache-able).
    // We need to move this data off of StyleResolverState and closer to the
    // objects it applies to. Possibly separating (immutable) inputs from (mutable) outputs.
    void setApplyPropertyToRegularStyle(bool isApply) { m_applyPropertyToRegularStyle = isApply; }
    void setApplyPropertyToVisitedLinkStyle(bool isApply) { m_applyPropertyToVisitedLinkStyle = isApply; }
    bool applyPropertyToRegularStyle() const { return m_applyPropertyToRegularStyle; }
    bool applyPropertyToVisitedLinkStyle() const { return m_applyPropertyToVisitedLinkStyle; }

    // Holds all attribute names found while applying "content" properties that contain an "attr()" value.
    Vector<AtomicString>& contentAttrValues() { return m_contentAttrValues; }

    void setLineHeightValue(CSSValue* value) { m_lineHeightValue = value; }
    CSSValue* lineHeightValue() { return m_lineHeightValue; }

    void cacheUserAgentBorderAndBackground() { m_cachedUAStyle = CachedUAStyle(style()); }
    const CachedUAStyle& cachedUAStyle() const { return m_cachedUAStyle; }

    ElementStyleResources& elementStyleResources() { return m_elementStyleResources; }
    const CSSToStyleMap& styleMap() const { return m_styleMap; }
    CSSToStyleMap& styleMap() { return m_styleMap; }

    // FIXME: Once styleImage can be made to not take a StyleResolverState
    // this convenience function should be removed. As-is, without this, call
    // sites are extremely verbose.
    PassRefPtr<StyleImage> styleImage(CSSPropertyID propertyId, CSSValue* value)
    {
        return m_elementStyleResources.styleImage(document().textLinkColors(), style()->visitedDependentColor(CSSPropertyColor), propertyId, value);
    }

    FontBuilder& fontBuilder() { return m_fontBuilder; }
    // FIXME: These exist as a primitive way to track mutations to font-related properties
    // on a RenderStyle. As designed, these are very error-prone, as some callers
    // set these directly on the RenderStyle w/o telling us. Presumably we'll
    // want to design a better wrapper around RenderStyle for tracking these mutations
    // and separate it from StyleResolverState.
    const FontDescription& parentFontDescription() { return m_parentStyle->fontDescription(); }
    void setZoom(float f) { m_fontBuilder.didChangeFontParameters(m_style->setZoom(f)); }
    void setEffectiveZoom(float f) { m_fontBuilder.didChangeFontParameters(m_style->setEffectiveZoom(f)); }
    void setWritingMode(WritingMode writingMode) { m_fontBuilder.didChangeFontParameters(m_style->setWritingMode(writingMode)); }
    void setTextOrientation(TextOrientation textOrientation) { m_fontBuilder.didChangeFontParameters(m_style->setTextOrientation(textOrientation)); }

    // SVG handles zooming in a different way compared to CSS. The whole document is scaled instead
    // of each individual length value in the render style / tree. CSSPrimitiveValue::computeLength*()
    // multiplies each resolved length with the zoom multiplier - so for SVG we need to disable that.
    // Though all CSS values that can be applied to outermost <svg> elements (width/height/border/padding...)
    // need to respect the scaling. RenderBox (the parent class of RenderSVGRoot) grabs values like
    // width/height/border/padding/... from the RenderStyle -> for SVG these values would never scale,
    // if we'd pass a 1.0 zoom factor everyhwere. So we only pass a zoom factor of 1.0 for specific
    // properties that are NOT allowed to scale within a zoomed SVG document (letter/word-spacing/font-size).
    bool useSVGZoomRules() const { return element() && element()->isSVGElement(); }

private:
    friend class StyleResolveScope;

    void initElement(Element*);

    ElementResolveContext m_elementContext;
    Document& m_document;

    // m_style is the primary output for each element's style resolve.
    RefPtr<RenderStyle> m_style;

    // m_parentStyle is not always just element->parentNode()->style()
    // so we keep it separate from m_elementContext.
    RefPtr<RenderStyle> m_parentStyle;

    OwnPtr<CSSAnimationUpdate> m_animationUpdate;

    // Required to ASSERT in applyProperties.
    // FIXME: Regions should not need special state on StyleResolverState
    // no other @rule does.
    RenderRegion* m_regionForStyling;

    bool m_applyPropertyToRegularStyle;
    bool m_applyPropertyToVisitedLinkStyle;

    CSSValue* m_lineHeightValue;

    FontBuilder m_fontBuilder;

    CachedUAStyle m_cachedUAStyle;

    ElementStyleResources m_elementStyleResources;
    // CSSToStyleMap is a pure-logic class and only contains
    // a back-pointer to this object.
    CSSToStyleMap m_styleMap;
    Vector<AtomicString> m_contentAttrValues;

    StyleRule* m_currentRule;
};

} // namespace WebCore

#endif // StyleResolverState_h
