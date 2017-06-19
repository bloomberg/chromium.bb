// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ContentLayerClientImpl_h
#define ContentLayerClientImpl_h

#include "cc/layers/content_layer_client.h"
#include "cc/layers/picture_layer.h"
#include "platform/PlatformExport.h"
#include "platform/graphics/GraphicsLayerClient.h"
#include "platform/graphics/paint/PaintChunk.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/Vector.h"

namespace blink {

class JSONArray;
class JSONObject;
struct RasterInvalidationTracking;

class PLATFORM_EXPORT ContentLayerClientImpl : public cc::ContentLayerClient {
  WTF_MAKE_NONCOPYABLE(ContentLayerClientImpl);
  USING_FAST_MALLOC(ContentLayerClientImpl);

 public:
  ContentLayerClientImpl(const PaintChunk& paint_chunk)
      : id_(paint_chunk.id),
        is_cacheable_(paint_chunk.is_cacheable),
        debug_name_(paint_chunk.id.client.DebugName()),
        cc_picture_layer_(cc::PictureLayer::Create(this)) {}

  ~ContentLayerClientImpl();

  void SetDisplayList(scoped_refptr<cc::DisplayItemList> cc_display_item_list) {
    cc_display_item_list_ = std::move(cc_display_item_list);
  }
  void SetPaintableRegion(const gfx::Rect& region) {
    paintable_region_ = region;
  }

  void AddPaintChunkDebugData(std::unique_ptr<JSONArray> json) {
    paint_chunk_debug_data_.push_back(std::move(json));
  }
  void ClearPaintChunkDebugData() { paint_chunk_debug_data_.clear(); }

  // cc::ContentLayerClient
  gfx::Rect PaintableRegion() override { return paintable_region_; }
  scoped_refptr<cc::DisplayItemList> PaintContentsToDisplayList(
      PaintingControlSetting) override {
    return cc_display_item_list_;
  }
  bool FillsBoundsCompletely() const override { return false; }
  size_t GetApproximateUnsharedMemoryUsage() const override {
    // TODO(jbroman): Actually calculate memory usage.
    return 0;
  }

  void ResetTrackedRasterInvalidations();
  RasterInvalidationTracking& EnsureRasterInvalidationTracking();

  void SetNeedsDisplayRect(const gfx::Rect& rect) {
    cc_picture_layer_->SetNeedsDisplayRect(rect);
  }

  std::unique_ptr<JSONObject> LayerAsJSON(LayerTreeFlags);

  scoped_refptr<cc::PictureLayer> CcPictureLayer() { return cc_picture_layer_; }

  bool Matches(const PaintChunk& paint_chunk) {
    return is_cacheable_ && paint_chunk.Matches(id_);
  }

  const String& DebugName() const { return debug_name_; }

 private:
  PaintChunk::Id id_;
  bool is_cacheable_;
  String debug_name_;
  scoped_refptr<cc::PictureLayer> cc_picture_layer_;
  scoped_refptr<cc::DisplayItemList> cc_display_item_list_;
  gfx::Rect paintable_region_;
  Vector<std::unique_ptr<JSONArray>> paint_chunk_debug_data_;
};

}  // namespace blink

#endif  // ContentLayerClientImpl_h
