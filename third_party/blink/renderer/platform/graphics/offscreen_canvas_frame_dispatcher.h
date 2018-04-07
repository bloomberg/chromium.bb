// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_OFFSCREEN_CANVAS_FRAME_DISPATCHER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_OFFSCREEN_CANVAS_FRAME_DISPATCHER_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

class StaticBitmapImage;

class OffscreenCanvasFrameDispatcherClient {
 public:
  virtual void BeginFrame() = 0;
};

class PLATFORM_EXPORT OffscreenCanvasFrameDispatcher {
 public:
  virtual ~OffscreenCanvasFrameDispatcher() = default;
  virtual void DispatchFrame(scoped_refptr<StaticBitmapImage>,
                             double commit_start_time,
                             const SkIRect& damage_rect) = 0;
  virtual void ReclaimResource(unsigned resource_id) = 0;
  virtual void SetNeedsBeginFrame(bool) = 0;
  virtual void SetSuspendAnimation(bool) = 0;
  virtual bool NeedsBeginFrame() const = 0;
  virtual bool IsAnimationSuspended() const = 0;

  virtual void Reshape(int width, int height) = 0;

  base::WeakPtr<OffscreenCanvasFrameDispatcher> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  OffscreenCanvasFrameDispatcherClient* Client() { return client_; }

 protected:
  OffscreenCanvasFrameDispatcher(OffscreenCanvasFrameDispatcherClient* client)
      : weak_ptr_factory_(this), client_(client) {}

 private:
  base::WeakPtrFactory<OffscreenCanvasFrameDispatcher> weak_ptr_factory_;
  OffscreenCanvasFrameDispatcherClient* client_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_OFFSCREEN_CANVAS_FRAME_DISPATCHER_H_
