// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INTERSECTION_OBSERVER_INTERSECTION_GEOMETRY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INTERSECTION_OBSERVER_INTERSECTION_GEOMETRY_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class Element;
class LayoutObject;

// Computes the intersection between an ancestor (root) element and a
// descendant (target) element, with overflow and CSS clipping applied.
// Optionally also checks whether the target is occluded or has visual
// effects applied.
//
// If the root argument to the constructor is null, computes the intersection
// of the target with the top-level frame viewport (AKA the "implicit root").
class CORE_EXPORT IntersectionGeometry {
 public:
  enum Flags {
    // These flags should passed to the constructor
    kShouldReportRootBounds = 1 << 0,
    kShouldComputeVisibility = 1 << 1,
    kShouldTrackFractionOfRoot = 1 << 2,
    kShouldUseReplacedContentRect = 1 << 3,
    kShouldConvertToCSSPixels = 1 << 4,

    // These flags will be computed
    kRootIsImplicit = 1 << 5,
    kIsVisible = 1 << 6
  };

  IntersectionGeometry(Element* root,
                       Element& target,
                       const Vector<Length>& root_margin,
                       const Vector<float>& thresholds,
                       unsigned flags);

  IntersectionGeometry(const IntersectionGeometry&) = default;
  ~IntersectionGeometry();

  bool ShouldReportRootBounds() const {
    return flags_ & kShouldReportRootBounds;
  }
  bool ShouldComputeVisibility() const {
    return flags_ & kShouldComputeVisibility;
  }
  bool ShouldTrackFractionOfRoot() const {
    return flags_ & kShouldTrackFractionOfRoot;
  }

  PhysicalRect TargetRect() const { return target_rect_; }
  PhysicalRect IntersectionRect() const { return intersection_rect_; }
  PhysicalRect RootRect() const { return root_rect_; }

  IntRect IntersectionIntRect() const {
    return PixelSnappedIntRect(intersection_rect_);
  }
  IntRect TargetIntRect() const { return PixelSnappedIntRect(target_rect_); }
  IntRect RootIntRect() const { return PixelSnappedIntRect(root_rect_); }

  double IntersectionRatio() const { return intersection_ratio_; }
  unsigned ThresholdIndex() const { return threshold_index_; }

  bool RootIsImplicit() const { return flags_ & kRootIsImplicit; }
  bool IsIntersecting() const { return threshold_index_ > 0; }
  bool IsVisible() const { return flags_ & kIsVisible; }

 private:
  void ComputeGeometry(Element* root_element,
                       Element& target_element,
                       const Vector<Length>& root_margin,
                       const Vector<float>& thresholds);
  PhysicalRect InitializeTargetRect(LayoutObject* target);
  PhysicalRect InitializeRootRect(LayoutObject* root,
                                  const Vector<Length>& margin);
  void ApplyRootMargin(PhysicalRect& rect, const Vector<Length>& margin);
  bool ClipToRoot(LayoutObject* root,
                  LayoutObject* target,
                  const PhysicalRect& root_rect,
                  PhysicalRect& intersection_rect);
  unsigned FirstThresholdGreaterThan(float ratio,
                                     const Vector<float>& thresholds) const;

  PhysicalRect target_rect_;
  PhysicalRect intersection_rect_;
  PhysicalRect root_rect_;
  unsigned flags_;
  double intersection_ratio_;
  unsigned threshold_index_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INTERSECTION_OBSERVER_INTERSECTION_GEOMETRY_H_
