// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_DELEGATED_INK_POINT_H_
#define COMPONENTS_VIZ_COMMON_DELEGATED_INK_POINT_H_

#include <string>

#include "base/time/time.h"
#include "components/viz/common/viz_common_export.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "ui/gfx/geometry/point_f.h"

namespace viz {

namespace mojom {
class DelegatedInkPointDataView;
}  // namespace mojom

// This class stores the information required to draw a single point of a
// delegated ink trail. When the WebAPI |updateInkTrailStartPoint| is called,
// the renderer requests that the browser begin sending these to viz. Viz
// will collect them, and then during |DrawAndSwap| will use the
// DelegatedInkPoints that have arrived from the browser along with the
// DelegatedInkMetadata that the renderer sent to draw a delegated ink trail on
// the screen, connected to the end of the already rendered ink stroke.
//
// Explainer for the feature:
// https://github.com/WICG/ink-enhancement/blob/master/README.md
class VIZ_COMMON_EXPORT DelegatedInkPoint {
 public:
  DelegatedInkPoint() = default;
  DelegatedInkPoint(const gfx::PointF& pt, base::TimeTicks timestamp)
      : point_(pt), timestamp_(timestamp) {}

  const gfx::PointF& point() const { return point_; }
  base::TimeTicks timestamp() const { return timestamp_; }
  std::string ToString() const;

 private:
  friend struct mojo::StructTraits<mojom::DelegatedInkPointDataView,
                                   DelegatedInkPoint>;

  // Location of the input event relative to the root window in device pixels.
  // Scale is device scale factor at time of input.
  gfx::PointF point_;

  // Timestamp from the input event.
  base::TimeTicks timestamp_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_DELEGATED_INK_POINT_H_
