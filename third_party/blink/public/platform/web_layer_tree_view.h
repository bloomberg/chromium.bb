/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_LAYER_TREE_VIEW_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_LAYER_TREE_VIEW_H_

#include "base/callback.h"
#include "cc/input/browser_controls_state.h"
#include "cc/layers/layer.h"
#include "cc/paint/element_id.h"
#include "cc/paint/paint_worklet_layer_painter.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_mutator.h"
#include "cc/trees/paint_holding_commit_trigger.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

class WebLayerTreeView {
 public:

  virtual ~WebLayerTreeView() = default;

  // View properties ---------------------------------------------------

  // Update browser controls permitted and current states
  virtual void UpdateBrowserControlsState(cc::BrowserControlsState constraints,
                                          cc::BrowserControlsState current,
                                          bool animate) {}

  // Input properties ---------------------------------------------------

  // Returns the FrameSinkId of the widget associated with this layer tree view.
  virtual viz::FrameSinkId GetFrameSinkId() { return viz::FrameSinkId(); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_LAYER_TREE_VIEW_H_
