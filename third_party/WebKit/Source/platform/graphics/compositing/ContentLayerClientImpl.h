// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ContentLayerClientImpl_h
#define ContentLayerClientImpl_h

#include "cc/layers/content_layer_client.h"
#include "cc/layers/picture_layer.h"
#include "platform/PlatformExport.h"
#include "platform/graphics/GraphicsLayerClient.h"
#include "platform/graphics/compositing/CompositedLayerRasterInvalidator.h"
#include "platform/graphics/paint/PaintChunk.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/Vector.h"

namespace blink {

class JSONArray;
class JSONObject;
class PaintArtifact;

class PLATFORM_EXPORT ContentLayerClientImpl : public cc::ContentLayerClient {
  WTF_MAKE_NONCOPYABLE(ContentLayerClientImpl);
  USING_FAST_MALLOC(ContentLayerClientImpl);

 public:
  ContentLayerClientImpl()
      : cc_picture_layer_(cc::PictureLayer::Create(this)),
        raster_invalidator_([this](const IntRect& rect) {
          cc_picture_layer_->SetNeedsDisplayRect(rect);
        }) {}
  ~ContentLayerClientImpl() override {}

  // cc::ContentLayerClient
  gfx::Rect PaintableRegion() override {
    return gfx::Rect(raster_invalidator_.LayerBounds().size());
  }
  scoped_refptr<cc::DisplayItemList> PaintContentsToDisplayList(
      PaintingControlSetting) override {
    return cc_display_item_list_;
  }
  bool FillsBoundsCompletely() const override { return false; }
  size_t GetApproximateUnsharedMemoryUsage() const override {
    // TODO(jbroman): Actually calculate memory usage.
    return 0;
  }

  void SetTracksRasterInvalidations(bool should_track) {
    raster_invalidator_.SetTracksRasterInvalidations(should_track);
  }

  bool Matches(const PaintChunk& paint_chunk) const {
    return raster_invalidator_.Matches(paint_chunk);
  }

  struct LayerAsJSONContext {
    LayerAsJSONContext(LayerTreeFlags flags) : flags(flags) {}

    const LayerTreeFlags flags;
    int next_transform_id = 1;
    std::unique_ptr<JSONArray> transforms_json;
    HashMap<const TransformPaintPropertyNode*, int> transform_id_map;
    HashMap<int, int> rendering_context_map;
  };
  std::unique_ptr<JSONObject> LayerAsJSON(LayerAsJSONContext&) const;

  scoped_refptr<cc::PictureLayer> UpdateCcPictureLayer(
      const PaintArtifact&,
      const gfx::Rect& layer_bounds,
      const Vector<const PaintChunk*>&,
      const PropertyTreeState&);

 private:
  scoped_refptr<cc::PictureLayer> cc_picture_layer_;
  scoped_refptr<cc::DisplayItemList> cc_display_item_list_;
  CompositedLayerRasterInvalidator raster_invalidator_;

  String debug_name_;
#ifndef NDEBUG
  std::unique_ptr<JSONArray> paint_chunk_debug_data_;
#endif
};

}  // namespace blink

#endif  // ContentLayerClientImpl_h
