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
#include "core/css/CSSValueList.h"
#if ENABLE(SVG)
#include "core/css/WebKitCSSSVGDocumentValue.h"
#endif
#include "core/dom/Element.h"
#include "core/rendering/style/BorderData.h"
#include "core/rendering/style/FillLayer.h"
#include "core/rendering/style/RenderStyle.h"
#include "core/rendering/style/StyleInheritedData.h"
#include "core/platform/graphics/filters/FilterOperations.h"
#include <wtf/HashMap.h>

namespace WebCore {

    class FillLayer;
    class FontDescription;
    class RenderRegion;
    class StyledElement;

    typedef HashMap<CSSPropertyID, RefPtr<CSSValue> > PendingImagePropertyMap;
#if ENABLE(SVG)
    typedef HashMap<FilterOperation*, RefPtr<WebKitCSSSVGDocumentValue> > PendingSVGDocumentMap;
#endif

    class StyleResolverState {
        WTF_MAKE_NONCOPYABLE(StyleResolverState);
    public:
        StyleResolverState()
        : m_element(0)
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
        , m_hasPendingShaders(false)
        , m_lineHeightValue(0)
        , m_fontDirty(false)
        , m_hasUAAppearance(false)
        , m_backgroundData(BackgroundFillLayer) { }

    public:
        void initElement(Element*);
        void initForStyleResolve(Document*, Element*, RenderStyle* parentStyle = 0, RenderRegion* regionForStyling = 0);
        void clear();

        Document* document() const { return m_element->document(); }
        Element* element() const { return m_element; }
        StyledElement* styledElement() const { return m_styledElement; }
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

        void setApplyPropertyToRegularStyle(bool isApply) { m_applyPropertyToRegularStyle = isApply; }
        void setApplyPropertyToVisitedLinkStyle(bool isApply) { m_applyPropertyToVisitedLinkStyle = isApply; }
        bool applyPropertyToRegularStyle() const { return m_applyPropertyToRegularStyle; }
        bool applyPropertyToVisitedLinkStyle() const { return m_applyPropertyToVisitedLinkStyle; }
        PendingImagePropertyMap& pendingImageProperties() { return m_pendingImageProperties; }
#if ENABLE(SVG)
        PendingSVGDocumentMap& pendingSVGDocuments() { return m_pendingSVGDocuments; }
#endif
        void setHasPendingShaders(bool hasPendingShaders) { m_hasPendingShaders = hasPendingShaders; }
        bool hasPendingShaders() const { return m_hasPendingShaders; }

        void setLineHeightValue(CSSValue* value) { m_lineHeightValue = value; }
        CSSValue* lineHeightValue() { return m_lineHeightValue; }
        void setFontDirty(bool isDirty) { m_fontDirty = isDirty; }
        bool fontDirty() const { return m_fontDirty; }

        void cacheBorderAndBackground();
        bool hasUAAppearance() const { return m_hasUAAppearance; }
        BorderData borderData() const { return m_borderData; }
        FillLayer backgroundData() const { return m_backgroundData; }
        Color backgroundColor() const { return m_backgroundColor; }

        const FontDescription& fontDescription() { return m_style->fontDescription(); }
        const FontDescription& parentFontDescription() { return m_parentStyle->fontDescription(); }
        void setFontDescription(const FontDescription& fontDescription) { m_fontDirty |= m_style->setFontDescription(fontDescription); }
        void setZoom(float f) { m_fontDirty |= m_style->setZoom(f); }
        void setEffectiveZoom(float f) { m_fontDirty |= m_style->setEffectiveZoom(f); }
        void setWritingMode(WritingMode writingMode) { m_fontDirty |= m_style->setWritingMode(writingMode); }
        void setTextOrientation(TextOrientation textOrientation) { m_fontDirty |= m_style->setTextOrientation(textOrientation); }

        bool useSVGZoomRules() const { return m_element && m_element->isSVGElement(); }

    private:
        // FIXME(bug 108563): to make it easier to review, these member
        // variables are public. However we should add methods to access
        // these variables.
        Element* m_element;
        RefPtr<RenderStyle> m_style;
        StyledElement* m_styledElement;
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

        PendingImagePropertyMap m_pendingImageProperties;
        bool m_hasPendingShaders;
#if ENABLE(SVG)
        PendingSVGDocumentMap m_pendingSVGDocuments;
#endif
        CSSValue* m_lineHeightValue;
        bool m_fontDirty;

        bool m_hasUAAppearance;
        BorderData m_borderData;
        FillLayer m_backgroundData;
        Color m_backgroundColor;
    };

} // namespace WebCore

#endif // StyleResolverState_h
