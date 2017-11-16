/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
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

#include "core/layout/svg/LayoutSVGResourceGradient.h"

#include <memory>
#include "platform/wtf/PtrUtil.h"

namespace blink {

LayoutSVGResourceGradient::LayoutSVGResourceGradient(SVGGradientElement* node)
    : LayoutSVGResourcePaintServer(node),
      should_collect_gradient_attributes_(true) {}

void LayoutSVGResourceGradient::RemoveAllClientsFromCache(
    bool mark_for_invalidation) {
  gradient_map_.clear();
  should_collect_gradient_attributes_ = true;
  MarkAllClientsForInvalidation(
      mark_for_invalidation ? kPaintInvalidation : kParentOnlyInvalidation);
}

void LayoutSVGResourceGradient::RemoveClientFromCache(
    LayoutObject* client,
    bool mark_for_invalidation) {
  DCHECK(client);
  gradient_map_.erase(client);
  MarkClientForInvalidation(client, mark_for_invalidation
                                        ? kPaintInvalidation
                                        : kParentOnlyInvalidation);
}

SVGPaintServer LayoutSVGResourceGradient::PreparePaintServer(
    const LayoutObject& object) {
  ClearInvalidationMask();

  // Validate gradient DOM state before building the actual
  // gradient. This should avoid tearing down the gradient we're
  // currently working on. Preferably the state validation should have
  // no side-effects though.
  if (should_collect_gradient_attributes_) {
    if (!CollectGradientAttributes())
      return SVGPaintServer::Invalid();
    should_collect_gradient_attributes_ = false;
  }

  // Spec: When the geometry of the applicable element has no width or height
  // and objectBoundingBox is specified, then the given effect (e.g. a gradient
  // or a filter) will be ignored.
  FloatRect object_bounding_box = object.ObjectBoundingBox();
  if (GradientUnits() == SVGUnitTypes::kSvgUnitTypeObjectboundingbox &&
      object_bounding_box.IsEmpty())
    return SVGPaintServer::Invalid();

  std::unique_ptr<GradientData>& gradient_data =
      gradient_map_.insert(&object, nullptr).stored_value->value;
  if (!gradient_data)
    gradient_data = WTF::WrapUnique(new GradientData);

  // Create gradient object
  if (!gradient_data->gradient) {
    gradient_data->gradient = BuildGradient();

    // We want the text bounding box applied to the gradient space transform
    // now, so the gradient shader can use it.
    if (GradientUnits() == SVGUnitTypes::kSvgUnitTypeObjectboundingbox &&
        !object_bounding_box.IsEmpty()) {
      gradient_data->userspace_transform.Translate(object_bounding_box.X(),
                                                   object_bounding_box.Y());
      gradient_data->userspace_transform.ScaleNonUniform(
          object_bounding_box.Width(), object_bounding_box.Height());
    }

    AffineTransform gradient_transform = CalculateGradientTransform();
    gradient_data->userspace_transform *= gradient_transform;
  }

  if (!gradient_data->gradient)
    return SVGPaintServer::Invalid();

  return SVGPaintServer(gradient_data->gradient,
                        gradient_data->userspace_transform);
}

bool LayoutSVGResourceGradient::IsChildAllowed(LayoutObject* child,
                                               const ComputedStyle&) const {
  if (!child->IsSVGResourceContainer())
    return false;

  return ToLayoutSVGResourceContainer(child)->IsSVGPaintServer();
}

GradientSpreadMethod LayoutSVGResourceGradient::PlatformSpreadMethodFromSVGType(
    SVGSpreadMethodType method) {
  switch (method) {
    case kSVGSpreadMethodUnknown:
    case kSVGSpreadMethodPad:
      return kSpreadMethodPad;
    case kSVGSpreadMethodReflect:
      return kSpreadMethodReflect;
    case kSVGSpreadMethodRepeat:
      return kSpreadMethodRepeat;
  }

  NOTREACHED();
  return kSpreadMethodPad;
}

}  // namespace blink
