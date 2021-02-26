// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/resize_observer/resize_observation.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_box_options.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_utilities.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_graphics_element.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

ResizeObservation::ResizeObservation(Element* target,
                                     ResizeObserver* observer,
                                     ResizeObserverBoxOptions observed_box)
    : target_(target),
      observer_(observer),
      observation_size_(0, 0),
      observed_box_(observed_box) {
  DCHECK(target_);
}

bool ResizeObservation::ObservationSizeOutOfSync() {
  if (observation_size_ == ComputeTargetSize())
    return false;

  // Skip resize observations on locked elements.
  if (UNLIKELY(target_ && DisplayLockUtilities::IsInLockedSubtreeCrossingFrames(
                              *target_))) {
    return false;
  }

  return true;
}

void ResizeObservation::SetObservationSize(const LayoutSize& observation_size) {
  observation_size_ = observation_size;
}

size_t ResizeObservation::TargetDepth() {
  unsigned depth = 0;
  for (Element* parent = target_; parent; parent = parent->parentElement())
    ++depth;
  return depth;
}

LayoutSize ResizeObservation::ComputeTargetSize() const {
  if (target_) {
    if (LayoutObject* layout_object = target_->GetLayoutObject()) {
      const ComputedStyle& style = layout_object->StyleRef();
      if (auto* svg_graphics_element =
              DynamicTo<SVGGraphicsElement>(target_.Get())) {
        LayoutSize bounding_box_size =
            LayoutSize(svg_graphics_element->GetBBox().Size());
        switch (observed_box_) {
          case ResizeObserverBoxOptions::BorderBox:
          case ResizeObserverBoxOptions::ContentBox:
            return bounding_box_size;
          case ResizeObserverBoxOptions::DevicePixelContentBox: {
            bounding_box_size.Scale(style.EffectiveZoom());
            LayoutSize snapped_device_pixel_content_box_size = LayoutSize(
                ResizeObserverUtilities::ComputeSnappedDevicePixelContentBox(
                    bounding_box_size, layout_object, style));

            return snapped_device_pixel_content_box_size;
          }
        }
      }
      if (!layout_object->IsBox())
        return LayoutSize();

      return LayoutSize(ResizeObserverUtilities::ComputeZoomAdjustedBox(
          observed_box_, layout_object, style));
    }
  }
  return LayoutSize();
}

void ResizeObservation::Trace(Visitor* visitor) const {
  visitor->Trace(target_);
  visitor->Trace(observer_);
}

}  // namespace blink
