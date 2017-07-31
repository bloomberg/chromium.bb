/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2008 Dirk Schulze <krit@webkit.org>
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

#include "core/layout/svg/LayoutSVGResourcePaintServer.h"

#include "core/layout/svg/SVGResources.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/style/ComputedStyle.h"
#include "platform/graphics/paint/PaintCanvas.h"
#include "platform/graphics/paint/PaintFlags.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "third_party/skia/include/core/SkPaint.h"

namespace blink {

SVGPaintServer::SVGPaintServer(Color color) : color_(color) {}

SVGPaintServer::SVGPaintServer(RefPtr<Gradient> gradient,
                               const AffineTransform& transform)
    : gradient_(std::move(gradient)),
      transform_(transform),
      color_(Color::kBlack) {}

SVGPaintServer::SVGPaintServer(RefPtr<Pattern> pattern,
                               const AffineTransform& transform)
    : pattern_(std::move(pattern)),
      transform_(transform),
      color_(Color::kBlack) {}

void SVGPaintServer::ApplyToPaintFlags(PaintFlags& flags, float alpha) {
  SkColor base_color = gradient_ || pattern_ ? SK_ColorBLACK : color_.Rgb();
  flags.setColor(ScaleAlpha(base_color, alpha));
  if (pattern_) {
    pattern_->ApplyToFlags(flags, AffineTransformToSkMatrix(transform_));
  } else if (gradient_) {
    gradient_->ApplyToFlags(flags, AffineTransformToSkMatrix(transform_));
  } else {
    flags.setShader(nullptr);
  }
}

void SVGPaintServer::PrependTransform(const AffineTransform& transform) {
  DCHECK(gradient_ || pattern_);
  transform_ = transform * transform_;
}

static SVGPaintDescription RequestPaint(const LayoutObject& object,
                                        const ComputedStyle& style,
                                        LayoutSVGResourceMode mode) {
  // If we have no style at all, ignore it.
  const SVGComputedStyle& svg_style = style.SvgStyle();

  // If we have no fill/stroke, return 0.
  if (mode == kApplyToFillMode) {
    if (!svg_style.HasFill())
      return SVGPaintDescription();
  } else {
    if (!svg_style.HasStroke())
      return SVGPaintDescription();
  }

  bool apply_to_fill = mode == kApplyToFillMode;
  SVGPaintType paint_type =
      apply_to_fill ? svg_style.FillPaintType() : svg_style.StrokePaintType();
  DCHECK_NE(paint_type, SVG_PAINTTYPE_NONE);

  Color color;
  bool has_color = false;
  switch (paint_type) {
    case SVG_PAINTTYPE_CURRENTCOLOR:
    case SVG_PAINTTYPE_URI_CURRENTCOLOR:
      // The keyword `currentcolor` takes its value from the value of the
      // `color` property on the same element.
      color = style.VisitedDependentColor(CSSPropertyColor);
      has_color = true;
      break;
    case SVG_PAINTTYPE_RGBCOLOR:
    case SVG_PAINTTYPE_URI_RGBCOLOR:
      color = apply_to_fill ? svg_style.FillPaintColor()
                            : svg_style.StrokePaintColor();
      has_color = true;
    default:
      break;
  }

  if (style.InsideLink() == EInsideLink::kInsideVisitedLink) {
    // FIXME: This code doesn't support the uri component of the visited link
    // paint, https://bugs.webkit.org/show_bug.cgi?id=70006
    SVGPaintType visited_paint_type =
        apply_to_fill ? svg_style.VisitedLinkFillPaintType()
                      : svg_style.VisitedLinkStrokePaintType();

    // For SVG_PAINTTYPE_CURRENTCOLOR, 'color' already contains the
    // 'visitedColor'.
    if (visited_paint_type < SVG_PAINTTYPE_URI_NONE &&
        visited_paint_type != SVG_PAINTTYPE_CURRENTCOLOR) {
      const Color& visited_color =
          apply_to_fill ? svg_style.VisitedLinkFillPaintColor()
                        : svg_style.VisitedLinkStrokePaintColor();
      color = Color(visited_color.Red(), visited_color.Green(),
                    visited_color.Blue(), color.Alpha());
      has_color = true;
    }
  }

  // If the primary resource is just a color, return immediately.
  if (paint_type < SVG_PAINTTYPE_URI_NONE) {
    // |paintType| will be either <current-color> or <rgb-color> here - both of
    // which will have a color.
    DCHECK(has_color);
    return SVGPaintDescription(color);
  }

  LayoutSVGResourcePaintServer* uri_resource = nullptr;
  if (SVGResources* resources =
          SVGResourcesCache::CachedResourcesForLayoutObject(&object))
    uri_resource = apply_to_fill ? resources->Fill() : resources->Stroke();

  // If the requested resource is not available, return the color resource or
  // 'none'.
  if (!uri_resource) {
    // The fallback is 'none'. (SVG2 say 'none' is implied when no fallback is
    // specified.)
    if (paint_type == SVG_PAINTTYPE_URI_NONE || !has_color)
      return SVGPaintDescription();

    return SVGPaintDescription(color);
  }

  // The paint server resource exists, though it may be invalid (pattern with
  // width/height=0). Return the fallback color to our caller so it can use it,
  // if preparePaintServer() on the resource container failed.
  if (has_color)
    return SVGPaintDescription(uri_resource, color);

  return SVGPaintDescription(uri_resource);
}

SVGPaintServer SVGPaintServer::RequestForLayoutObject(
    const LayoutObject& layout_object,
    const ComputedStyle& style,
    LayoutSVGResourceMode resource_mode) {
  DCHECK(resource_mode == kApplyToFillMode ||
         resource_mode == kApplyToStrokeMode);

  SVGPaintDescription paint_description =
      RequestPaint(layout_object, style, resource_mode);
  if (!paint_description.is_valid)
    return Invalid();
  if (!paint_description.resource)
    return SVGPaintServer(paint_description.color);
  SVGPaintServer paint_server =
      paint_description.resource->PreparePaintServer(layout_object);
  if (paint_server.IsValid())
    return paint_server;
  if (paint_description.has_fallback)
    return SVGPaintServer(paint_description.color);
  return Invalid();
}

bool SVGPaintServer::ExistsForLayoutObject(
    const LayoutObject& layout_object,
    const ComputedStyle& style,
    LayoutSVGResourceMode resource_mode) {
  return RequestPaint(layout_object, style, resource_mode).is_valid;
}

LayoutSVGResourcePaintServer::LayoutSVGResourcePaintServer(SVGElement* element)
    : LayoutSVGResourceContainer(element) {}

LayoutSVGResourcePaintServer::~LayoutSVGResourcePaintServer() {}

SVGPaintDescription LayoutSVGResourcePaintServer::RequestPaintDescription(
    const LayoutObject& layout_object,
    const ComputedStyle& style,
    LayoutSVGResourceMode resource_mode) {
  return RequestPaint(layout_object, style, resource_mode);
}

}  // namespace blink
