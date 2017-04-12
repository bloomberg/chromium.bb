/**
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
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

#ifndef SVGLayoutSupport_h
#define SVGLayoutSupport_h

#include "core/layout/LayoutObject.h"
#include "core/style/SVGComputedStyleDefs.h"
#include "platform/graphics/DashArray.h"
#include "platform/transforms/AffineTransform.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class AffineTransform;
class FloatPoint;
class FloatRect;
class LayoutRect;
class LayoutGeometryMap;
class LayoutBoxModelObject;
class LayoutObject;
class ComputedStyle;
class LayoutSVGRoot;
class SVGLengthContext;
class StrokeData;
class TransformState;

class CORE_EXPORT SVGLayoutSupport {
  STATIC_ONLY(SVGLayoutSupport);

 public:
  // Shares child layouting code between
  // LayoutSVGRoot/LayoutSVG(Hidden)Container
  static void LayoutChildren(LayoutObject*,
                             bool force_layout,
                             bool screen_scaling_factor_changed,
                             bool layout_size_changed);

  // Layout resources used by this node.
  static void LayoutResourcesIfNeeded(const LayoutObject*);

  // Helper function determining whether overflow is hidden.
  static bool IsOverflowHidden(const LayoutObject*);

  // Adjusts the visualRect in combination with filter, clipper and masker
  // in local coordinates.
  static void AdjustVisualRectWithResources(const LayoutObject*, FloatRect&);

  // Determine if the LayoutObject references a filter resource object.
  static bool HasFilterResource(const LayoutObject&);

  // Determines whether the passed point lies in a clipping area
  static bool PointInClippingArea(const LayoutObject&, const FloatPoint&);

  // Transform |pointInParent| to |object|'s user-space and check if it is
  // within the clipping area. Returns false if the transform is singular or
  // the point is outside the clipping area.
  static bool TransformToUserSpaceAndCheckClipping(
      const LayoutObject&,
      const AffineTransform& local_transform,
      const FloatPoint& point_in_parent,
      FloatPoint& local_point);

  static void ComputeContainerBoundingBoxes(const LayoutObject* container,
                                            FloatRect& object_bounding_box,
                                            bool& object_bounding_box_valid,
                                            FloatRect& stroke_bounding_box,
                                            FloatRect& local_visual_rect);

  // Important functions used by nearly all SVG layoutObjects centralizing
  // coordinate transformations / visual rect calculations
  static FloatRect LocalVisualRect(const LayoutObject&);
  static LayoutRect VisualRectInAncestorSpace(
      const LayoutObject&,
      const LayoutBoxModelObject& ancestor);
  static LayoutRect TransformVisualRect(const LayoutObject&,
                                        const AffineTransform&,
                                        const FloatRect&);
  static bool MapToVisualRectInAncestorSpace(
      const LayoutObject&,
      const LayoutBoxModelObject* ancestor,
      const FloatRect& local_visual_rect,
      LayoutRect& result_rect,
      VisualRectFlags = kDefaultVisualRectFlags);
  static void MapLocalToAncestor(const LayoutObject*,
                                 const LayoutBoxModelObject* ancestor,
                                 TransformState&,
                                 MapCoordinatesFlags);
  static void MapAncestorToLocal(const LayoutObject&,
                                 const LayoutBoxModelObject* ancestor,
                                 TransformState&,
                                 MapCoordinatesFlags);
  static const LayoutObject* PushMappingToContainer(
      const LayoutObject*,
      const LayoutBoxModelObject* ancestor_to_stop_at,
      LayoutGeometryMap&);

  // Shared between SVG layoutObjects and resources.
  static void ApplyStrokeStyleToStrokeData(StrokeData&,
                                           const ComputedStyle&,
                                           const LayoutObject&,
                                           float dash_scale_factor);

  static DashArray ResolveSVGDashArray(const SVGDashArray&,
                                       const ComputedStyle&,
                                       const SVGLengthContext&);

  // Determines if any ancestor has adjusted the scale factor.
  static bool ScreenScaleFactorChanged(const LayoutObject*);

  // Determines if any ancestor's layout size has changed.
  static bool LayoutSizeOfNearestViewportChanged(const LayoutObject*);

  // FIXME: These methods do not belong here.
  static const LayoutSVGRoot* FindTreeRootObject(const LayoutObject*);

  // Helper method for determining if a LayoutObject marked as text (isText()==
  // true) can/will be laid out as part of a <text>.
  static bool IsLayoutableTextNode(const LayoutObject*);

  // Determines whether a svg node should isolate or not based on ComputedStyle.
  static bool WillIsolateBlendingDescendantsForStyle(const ComputedStyle&);
  static bool WillIsolateBlendingDescendantsForObject(const LayoutObject*);
  template <typename LayoutObjectType>
  static bool ComputeHasNonIsolatedBlendingDescendants(const LayoutObjectType*);
  static bool IsIsolationRequired(const LayoutObject*);

  static AffineTransform DeprecatedCalculateTransformToLayer(
      const LayoutObject*);
  static float CalculateScreenFontSizeScalingFactor(const LayoutObject*);

  static LayoutObject* FindClosestLayoutSVGText(LayoutObject*,
                                                const FloatPoint&);

 private:
  static void UpdateObjectBoundingBox(FloatRect& object_bounding_box,
                                      bool& object_bounding_box_valid,
                                      LayoutObject* other,
                                      FloatRect other_bounding_box);
};

class SubtreeContentTransformScope {
  STACK_ALLOCATED();

 public:
  SubtreeContentTransformScope(const AffineTransform&);
  ~SubtreeContentTransformScope();

  static AffineTransform CurrentContentTransformation() {
    return AffineTransform(current_content_transformation_);
  }

 private:
  static AffineTransform::Transform current_content_transformation_;
  AffineTransform saved_content_transformation_;
};

// The following enumeration is used to optimize cases where the scale is known
// to be invariant (see: LayoutSVGContainer::layout and LayoutSVGroot). The
// value 'Full' can be used in the general case when the scale change is
// unknown, or known to change.
enum class SVGTransformChange {
  kNone,
  kScaleInvariant,
  kFull,
};

// Helper for computing ("classifying") a change to a transform using the
// categoies defined above.
class SVGTransformChangeDetector {
  STACK_ALLOCATED();

 public:
  explicit SVGTransformChangeDetector(const AffineTransform& previous)
      : previous_transform_(previous) {}

  SVGTransformChange ComputeChange(const AffineTransform& current) {
    if (previous_transform_ == current)
      return SVGTransformChange::kNone;
    if (ScaleReference(previous_transform_) == ScaleReference(current))
      return SVGTransformChange::kScaleInvariant;
    return SVGTransformChange::kFull;
  }

 private:
  static std::pair<double, double> ScaleReference(
      const AffineTransform& transform) {
    return std::make_pair(transform.XScaleSquared(), transform.YScaleSquared());
  }
  AffineTransform previous_transform_;
};

template <typename LayoutObjectType>
bool SVGLayoutSupport::ComputeHasNonIsolatedBlendingDescendants(
    const LayoutObjectType* object) {
  for (LayoutObject* child = object->FirstChild(); child;
       child = child->NextSibling()) {
    if (child->IsBlendingAllowed() && child->Style()->HasBlendMode())
      return true;
    if (child->HasNonIsolatedBlendingDescendants() &&
        !WillIsolateBlendingDescendantsForObject(child))
      return true;
  }
  return false;
}

}  // namespace blink

#endif  // SVGLayoutSupport_h
