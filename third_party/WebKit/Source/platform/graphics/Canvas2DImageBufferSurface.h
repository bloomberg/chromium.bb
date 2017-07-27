/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Canvas2DImageBufferSurface_h
#define Canvas2DImageBufferSurface_h

#include "platform/graphics/Canvas2DLayerBridge.h"
#include "platform/graphics/ImageBufferSurface.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

// This shim is necessary because ImageBufferSurfaces are not allowed to be
// RefCounted.
class Canvas2DImageBufferSurface final : public ImageBufferSurface {
 public:
  Canvas2DImageBufferSurface(
      const IntSize& size,
      int msaa_sample_count,
      OpacityMode opacity_mode,
      Canvas2DLayerBridge::AccelerationMode acceleration_mode,
      const CanvasColorParams& color_params)
      : ImageBufferSurface(size, opacity_mode, color_params),
        layer_bridge_(AdoptRef(new Canvas2DLayerBridge(size,
                                                       msaa_sample_count,
                                                       opacity_mode,
                                                       acceleration_mode,
                                                       color_params))) {
    Init();
  }

  Canvas2DImageBufferSurface(PassRefPtr<Canvas2DLayerBridge> bridge,
                             const IntSize& size)
      : ImageBufferSurface(size,
                           bridge->GetOpacityMode(),
                           bridge->color_params()),
        layer_bridge_(std::move(bridge)) {
    Init();
  }

  ~Canvas2DImageBufferSurface() override { layer_bridge_->BeginDestruction(); }

  // ImageBufferSurface implementation
  void FinalizeFrame() override { layer_bridge_->FinalizeFrame(); }
  void DoPaintInvalidation(const FloatRect& dirty_rect) override {
    layer_bridge_->DoPaintInvalidation(dirty_rect);
  }
  void WillOverwriteCanvas() override { layer_bridge_->WillOverwriteCanvas(); }
  PaintCanvas* Canvas() override { return layer_bridge_->Canvas(); }
  void DisableDeferral(DisableDeferralReason reason) override {
    layer_bridge_->DisableDeferral(reason);
  }
  bool IsValid() const override { return layer_bridge_->CheckSurfaceValid(); }
  bool Restore() override { return layer_bridge_->RestoreSurface(); }
  WebLayer* Layer() const override { return layer_bridge_->Layer(); }
  bool IsAccelerated() const override { return layer_bridge_->IsAccelerated(); }
  void SetFilterQuality(SkFilterQuality filter_quality) override {
    layer_bridge_->SetFilterQuality(filter_quality);
  }
  void SetIsHidden(bool hidden) override { layer_bridge_->SetIsHidden(hidden); }
  void SetImageBuffer(ImageBuffer* image_buffer) override {
    layer_bridge_->SetImageBuffer(image_buffer);
  }
  void DidDraw(const FloatRect& rect) override { layer_bridge_->DidDraw(rect); }
  void Flush(FlushReason) override { layer_bridge_->Flush(); }
  void FlushGpu(FlushReason) override { layer_bridge_->FlushGpu(); }
  bool WritePixels(const SkImageInfo& orig_info,
                   const void* pixels,
                   size_t row_bytes,
                   int x,
                   int y) override {
    return layer_bridge_->WritePixels(orig_info, pixels, row_bytes, x, y);
  }

  RefPtr<StaticBitmapImage> NewImageSnapshot(AccelerationHint hint,
                                             SnapshotReason reason) override {
    return layer_bridge_->NewImageSnapshot(hint, reason);
  }

 private:
  void Init() {
    Clear();
    if (IsValid())
      layer_bridge_->Flush();
  }

  RefPtr<Canvas2DLayerBridge> layer_bridge_;
};

}  // namespace blink

#endif
