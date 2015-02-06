/**
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2012 Zoltan Herczeg <zherczeg@webkit.org>.
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

#ifndef SVGRenderingContext_h
#define SVGRenderingContext_h

#include "core/paint/CompositingRecorder.h"
#include "core/paint/FloatClipRecorder.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/svg/RenderSVGResourceClipper.h"
#include "platform/graphics/paint/ClipPathRecorder.h"
#include "platform/transforms/AffineTransform.h"

namespace blink {

class LayoutObject;
class RenderSVGResourceFilter;
class RenderSVGResourceMasker;
class SVGResources;

class SubtreeContentTransformScope {
public:
    SubtreeContentTransformScope(const AffineTransform&);
    ~SubtreeContentTransformScope();

private:
    AffineTransform m_savedContentTransformation;
};

class SVGRenderingContext {
    STACK_ALLOCATED();
public:
    SVGRenderingContext(LayoutObject& object, const PaintInfo& paintInfo)
        : m_object(&object)
        , m_paintInfo(paintInfo)
        , m_originalPaintInfo(&paintInfo)
        , m_filter(nullptr)
        , m_clipper(nullptr)
        , m_clipperState(RenderSVGResourceClipper::ClipperNotApplied)
        , m_masker(nullptr)
#if ENABLE(ASSERT)
        , m_applyClipMaskAndFilterIfNecessaryCalled(false)
#endif
    { }

    ~SVGRenderingContext();

    PaintInfo& paintInfo() { return m_paintInfo; }

    // Return true if these operations aren't necessary or if they are successfully applied.
    bool applyClipMaskAndFilterIfNecessary();

    static void renderSubtree(GraphicsContext*, LayoutObject*);

    static float calculateScreenFontSizeScalingFactor(const LayoutObject*);

private:
    void applyCompositingIfNecessary();

    // Return true if no clipping is necessary or if the clip is successfully applied.
    bool applyClipIfNecessary(SVGResources*);

    // Return true if no masking is necessary or if the mask is successfully applied.
    bool applyMaskIfNecessary(SVGResources*);

    // Return true if no filtering is necessary or if the filter is successfully applied.
    bool applyFilterIfNecessary(SVGResources*);

    bool isIsolationInstalled() const;

    RawPtrWillBeMember<LayoutObject> m_object;
    PaintInfo m_paintInfo;
    const PaintInfo* m_originalPaintInfo;
    RawPtrWillBeMember<RenderSVGResourceFilter> m_filter;
    RawPtrWillBeMember<RenderSVGResourceClipper> m_clipper;
    RenderSVGResourceClipper::ClipperState m_clipperState;
    RawPtrWillBeMember<RenderSVGResourceMasker> m_masker;
    OwnPtr<CompositingRecorder> m_compositingRecorder;
    OwnPtr<FloatClipRecorder> m_clipRecorder;
    OwnPtr<ClipPathRecorder> m_clipPathRecorder;
#if ENABLE(ASSERT)
    bool m_applyClipMaskAndFilterIfNecessaryCalled;
#endif
};

} // namespace blink

#endif // SVGRenderingContext_h
