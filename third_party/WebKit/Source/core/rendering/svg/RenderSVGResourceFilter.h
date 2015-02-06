/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
 */

#ifndef RenderSVGResourceFilter_h
#define RenderSVGResourceFilter_h

#include "core/rendering/svg/RenderSVGResourceContainer.h"
#include "core/svg/SVGFilterElement.h"
#include "core/svg/graphics/filters/SVGFilter.h"
#include "core/svg/graphics/filters/SVGFilterBuilder.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DisplayItemList.h"

namespace blink {

class FilterData final : public NoBaseWillBeGarbageCollectedFinalized<FilterData> {
    WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED;
public:
    /*
     * The state transitions should follow the following:
     * Initial -> RecordingContent -> ReadyToPaint -> PaintingFilter -> ReadyToPaint
     *                                                   |     ^
     *                                                   v     |
     *                                              PaintingFilterCycle
     */
    enum FilterDataState {
        Initial,
        RecordingContent,
        ReadyToPaint,
        PaintingFilter,
        PaintingFilterCycleDetected
    };

    static PassOwnPtrWillBeRawPtr<FilterData> create()
    {
        return adoptPtrWillBeNoop(new FilterData());
    }

    void trace(Visitor*);

    RefPtrWillBeMember<SVGFilter> filter;
    RefPtrWillBeMember<SVGFilterBuilder> builder;
    OwnPtr<DisplayItemList> m_displayItemList;
    OwnPtr<GraphicsContext> m_context;
    FloatRect boundaries;
    FilterDataState m_state;

private:
    FilterData() : m_state(Initial) { }
};

class RenderSVGResourceFilter final : public RenderSVGResourceContainer {
public:
    explicit RenderSVGResourceFilter(SVGFilterElement*);

    virtual ~RenderSVGResourceFilter();
    virtual void destroy() override;

    virtual bool isChildAllowed(LayoutObject*, const RenderStyle&) const override;

    virtual const char* renderName() const override { return "RenderSVGResourceFilter"; }
    virtual bool isOfType(LayoutObjectType type) const override { return type == LayoutObjectSVGResourceFilter || RenderSVGResourceContainer::isOfType(type); }

    virtual void removeAllClientsFromCache(bool markForInvalidation = true) override;
    virtual void removeClientFromCache(LayoutObject*, bool markForInvalidation = true) override;

    // Returns the context that should be used to paint the filter contents, or
    // null if the content should not be recorded.
    GraphicsContext* prepareEffect(LayoutObject*, GraphicsContext*);
    void finishEffect(LayoutObject*, GraphicsContext*);

    FloatRect resourceBoundingBox(const LayoutObject*);

    PassRefPtrWillBeRawPtr<SVGFilterBuilder> buildPrimitives(SVGFilter*);

    SVGUnitTypes::SVGUnitType filterUnits() const { return toSVGFilterElement(element())->filterUnits()->currentValue()->enumValue(); }
    SVGUnitTypes::SVGUnitType primitiveUnits() const { return toSVGFilterElement(element())->primitiveUnits()->currentValue()->enumValue(); }

    void primitiveAttributeChanged(LayoutObject*, const QualifiedName&);

    static const RenderSVGResourceType s_resourceType = FilterResourceType;
    virtual RenderSVGResourceType resourceType() const override { return s_resourceType; }

private:
    typedef WillBePersistentHeapHashMap<LayoutObject*, OwnPtrWillBeMember<FilterData>> FilterMap;
    FilterMap m_filter;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(RenderSVGResourceFilter, isSVGResourceFilter());

}

#endif
