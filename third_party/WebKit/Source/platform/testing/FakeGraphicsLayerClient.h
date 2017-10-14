// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FakeGraphicsLayerClient_h
#define FakeGraphicsLayerClient_h

#include "platform/graphics/GraphicsLayerClient.h"

namespace blink {

// A simple GraphicsLayerClient implementation suitable for use in unit tests.
class FakeGraphicsLayerClient : public GraphicsLayerClient {
 public:
  // GraphicsLayerClient implementation.
  IntRect ComputeInterestRect(const GraphicsLayer*,
                              const IntRect&) const override {
    return IntRect();
  }
  String DebugName(const GraphicsLayer*) const override { return String(); }
  bool IsTrackingRasterInvalidations() const override {
    return is_tracking_raster_invalidations_;
  }
  bool NeedsRepaint(const GraphicsLayer&) const override {
    return needs_repaint_;
  }
  void PaintContents(const GraphicsLayer*,
                     GraphicsContext&,
                     GraphicsLayerPaintingPhase,
                     const IntRect&) const override {}

  void SetIsTrackingRasterInvalidations(bool is_tracking_raster_invalidations) {
    is_tracking_raster_invalidations_ = is_tracking_raster_invalidations;
  }

  void SetNeedsRepaint(bool needs_repaint) { needs_repaint_ = needs_repaint; }

 private:
  bool is_tracking_raster_invalidations_ = false;
  bool needs_repaint_ = false;
};

}  // namespace blink

#endif  // FakeGraphicsLayerClient_h
