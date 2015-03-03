/*
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
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

#ifndef LayoutSVGResourceClipper_h
#define LayoutSVGResourceClipper_h

#include "core/layout/svg/LayoutSVGResourceContainer.h"
#include "core/svg/SVGClipPathElement.h"

class SkPicture;

namespace blink {

class LayoutSVGResourceClipper final : public LayoutSVGResourceContainer {
public:
    enum ClipperState {
        ClipperNotApplied,
        ClipperAppliedPath,
        ClipperAppliedMask
    };

    explicit LayoutSVGResourceClipper(SVGClipPathElement*);
    virtual ~LayoutSVGResourceClipper();

    virtual const char* name() const override { return "LayoutSVGResourceClipper"; }

    virtual void removeAllClientsFromCache(bool markForInvalidation = true) override;
    virtual void removeClientFromCache(LayoutObject*, bool markForInvalidation = true) override;

    // FIXME: Filters are also stateful resources that could benefit from having their state managed
    //        on the caller stack instead of the current hashmap. We should look at refactoring these
    //        into a general interface that can be shared.
    bool applyStatefulResource(LayoutObject*, GraphicsContext*&, ClipperState&);
    void postApplyStatefulResource(LayoutObject*, GraphicsContext*&, ClipperState&);

    // clipPath can be clipped too, but don't have a boundingBox or paintInvalidationRect. So we can't call
    // applyResource directly and use the rects from the object, since they are empty for LayoutSVGResources
    // FIXME: We made applyClippingToContext public because we cannot call applyResource on HTML elements (it asserts on LayoutObject::objectBoundingBox)
    bool applyClippingToContext(LayoutObject*, const FloatRect&, const FloatRect&, GraphicsContext*, ClipperState&);

    FloatRect resourceBoundingBox(const LayoutObject*);

    static const LayoutSVGResourceType s_resourceType = ClipperResourceType;
    virtual LayoutSVGResourceType resourceType() const override { return s_resourceType; }

    bool hitTestClipContent(const FloatRect&, const FloatPoint&);

    SVGUnitTypes::SVGUnitType clipPathUnits() const { return toSVGClipPathElement(element())->clipPathUnits()->currentValue()->enumValue(); }

private:
    bool tryPathOnlyClipping(DisplayItemClient, GraphicsContext*, const AffineTransform&, const FloatRect&);
    void drawClipMaskContent(GraphicsContext*, DisplayItemClient, const FloatRect& targetBoundingBox);
    PassRefPtr<const SkPicture> createPicture();
    void calculateClipContentPaintInvalidationRect();

    RefPtr<const SkPicture> m_clipContentPicture;
    FloatRect m_clipBoundaries;

    // Reference cycle detection.
    bool m_inClipExpansion;
};

DEFINE_LAYOUT_SVG_RESOURCE_TYPE_CASTS(LayoutSVGResourceClipper, ClipperResourceType);

}

#endif
