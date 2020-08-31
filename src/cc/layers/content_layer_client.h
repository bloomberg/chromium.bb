// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_CONTENT_LAYER_CLIENT_H_
#define CC_LAYERS_CONTENT_LAYER_CLIENT_H_

#include <stddef.h>

#include "cc/cc_export.h"
#include "cc/paint/display_item_list.h"

namespace gfx {
class Rect;
}

namespace cc {

class CC_EXPORT ContentLayerClient {
 public:
  enum PaintingControlSetting {
    PAINTING_BEHAVIOR_NORMAL,
    PAINTING_BEHAVIOR_NORMAL_FOR_TEST,
    DISPLAY_LIST_CACHING_DISABLED,
    SUBSEQUENCE_CACHING_DISABLED,
    PARTIAL_INVALIDATION,
  };

  // The paintable region is the rectangular region, within the bounds of the
  // layer this client paints, that the client is capable of painting via
  // paintContents(). Calling paintContents() will return a DisplayItemList
  // that is guaranteed valid only within this region.
  virtual gfx::Rect PaintableRegion() = 0;

  // Paints the content area for the layer, typically dirty rects submitted
  // to the layer itself, into a DisplayItemList that it returns. The
  // PaintingControlSetting enum controls painting to isolate different
  // components in performance tests.
  virtual scoped_refptr<DisplayItemList> PaintContentsToDisplayList(
      PaintingControlSetting painting_control) = 0;

  // If true the layer may skip clearing the background before rasterizing,
  // because it will cover any uncleared data with content.
  virtual bool FillsBoundsCompletely() const = 0;

  // Returns an estimate of the current memory usage within this object,
  // excluding memory shared with painting artifacts (i.e.,
  // DisplayItemList). Should be invoked after PaintContentsToDisplayList,
  // so that the result includes data cached internally during painting.
  virtual size_t GetApproximateUnsharedMemoryUsage() const = 0;

 protected:
  virtual ~ContentLayerClient() {}
};

}  // namespace cc

#endif  // CC_LAYERS_CONTENT_LAYER_CLIENT_H_
