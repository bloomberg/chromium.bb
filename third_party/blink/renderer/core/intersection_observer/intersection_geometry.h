// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INTERSECTION_OBSERVER_INTERSECTION_GEOMETRY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INTERSECTION_OBSERVER_INTERSECTION_GEOMETRY_H_

#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class Element;
class IntersectionObserverEntry;
class LayoutObject;

// Computes the intersection between an ancestor (root) element and a
// descendant (target) element, with overflow and CSS clipping applied.
// Optionally also checks whether the target is occluded or has visual
// effects applied.
//
// If the root argument to the constructor is null, computes the intersection
// of the target with the top-level frame viewport (AKA the "implicit root").
class IntersectionGeometry {
  STACK_ALLOCATED();

 public:
  enum Flags {
    // These flags should passed to the constructor
    kShouldReportRootBounds = 1 << 0,
    kShouldComputeVisibility = 1 << 1,
    kShouldTrackFractionOfRoot = 1 << 2,

    // These flags will be computed
    kRootIsImplicit = 1 << 3,
    kIsVisible = 1 << 4
  };

  IntersectionGeometry(Element* root,
                       Element& target,
                       const Vector<Length>& root_margin,
                       const Vector<float>& thresholds,
                       unsigned flags);
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

  LayoutObject* Root() const { return root_; }
  LayoutObject* Target() const { return target_; }

  // Client rect in the coordinate system of the frame containing target.
  LayoutRect TargetRect() const { return target_rect_; }
  // Target rect in CSS pixels
  LayoutRect UnZoomedTargetRect() const;

  // Client rect in the coordinate system of the frame containing target.
  LayoutRect IntersectionRect() const { return intersection_rect_; }
  // Intersection rect in CSS pixels
  LayoutRect UnZoomedIntersectionRect() const;

  // Client rect in the coordinate system of the frame containing root.
  LayoutRect RootRect() const { return root_rect_; }
  // Root rect in CSS pixels
  LayoutRect UnZoomedRootRect() const;

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

  IntersectionObserverEntry* CreateEntry(Element* target,
                                         DOMHighResTimeStamp timestamp);

 private:
  void ComputeGeometry();
  bool CanComputeGeometry(Element* root, Element& target) const;
  void InitializeTargetRect();
  void InitializeRootRect();
  bool ClipToRoot();
  void MapTargetRectToTargetFrameCoordinates();
  void MapRootRectToRootFrameCoordinates();
  void MapIntersectionRectToTargetFrameCoordinates();
  void ApplyRootMargin();
  unsigned FirstThresholdGreaterThan(float ratio) const;
  void ComputeVisibility();

  LayoutObject* root_;
  LayoutObject* target_;
  const Vector<Length>& root_margin_;
  const Vector<float>& thresholds_;
  LayoutRect target_rect_;
  LayoutRect intersection_rect_;
  LayoutRect root_rect_;
  unsigned flags_;
  double intersection_ratio_;
  unsigned threshold_index_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INTERSECTION_OBSERVER_INTERSECTION_GEOMETRY_H_
