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
class PaintArtifact;
struct RasterInvalidationTracking;

class PLATFORM_EXPORT ContentLayerClientImpl : public cc::ContentLayerClient {
  WTF_MAKE_NONCOPYABLE(ContentLayerClientImpl);
  USING_FAST_MALLOC(ContentLayerClientImpl);

 public:
  ContentLayerClientImpl()
      : cc_picture_layer_(cc::PictureLayer::Create(this)),
        layer_state_(nullptr, nullptr, nullptr) {}
  ~ContentLayerClientImpl() override {}

  // cc::ContentLayerClient
  gfx::Rect PaintableRegion() override {
    return gfx::Rect(layer_bounds_.size());
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

  void SetTracksRasterInvalidations(bool);

  std::unique_ptr<JSONObject> LayerAsJSON(LayerTreeFlags);

  scoped_refptr<cc::PictureLayer> UpdateCcPictureLayer(
      const PaintArtifact&,
      const gfx::Rect& layer_bounds,
      const Vector<const PaintChunk*>&,
      const PropertyTreeState& layer_state);

  bool Matches(const PaintChunk& paint_chunk) {
    return paint_chunks_info_.size() && paint_chunks_info_[0].is_cacheable &&
           paint_chunk.Matches(paint_chunks_info_[0].id);
  }

 private:
  friend class ContentLayerClientImplTest;
  const Vector<RasterInvalidationInfo>& TrackedRasterInvalidations() const;

  struct PaintChunkInfo {
    PaintChunkInfo(const IntRect& bounds, const PaintChunk& chunk)
        : bounds_in_layer(bounds),
          id(chunk.id),
          is_cacheable(chunk.is_cacheable),
          properties(chunk.properties) {}

    bool Matches(const PaintChunk& new_chunk) const {
      return is_cacheable && new_chunk.Matches(id);
    }

    IntRect bounds_in_layer;
    PaintChunk::Id id;
    bool is_cacheable;
    PaintChunkProperties properties;
  };

  IntRect MapRasterInvalidationRectFromChunkToLayer(const FloatRect&,
                                                    const PaintChunk&) const;

  void GenerateRasterInvalidations(
      const Vector<const PaintChunk*>& new_chunks,
      const Vector<PaintChunkInfo>& new_chunks_info);
  size_t MatchNewChunkToOldChunk(const PaintChunk& new_chunk, size_t old_index);
  void AddDisplayItemRasterInvalidations(const PaintChunk&);
  void InvalidateRasterForNewChunk(const PaintChunkInfo&,
                                   PaintInvalidationReason);
  void InvalidateRasterForOldChunk(const PaintChunkInfo&,
                                   PaintInvalidationReason);
  void InvalidateRasterForWholeLayer();

  scoped_refptr<cc::PictureLayer> cc_picture_layer_;
  scoped_refptr<cc::DisplayItemList> cc_display_item_list_;
  gfx::Rect layer_bounds_;
  PropertyTreeState layer_state_;

  Vector<PaintChunkInfo> paint_chunks_info_;
  String debug_name_;
#ifndef NDEBUG
  std::unique_ptr<JSONArray> paint_chunk_debug_data_;
#endif

  struct RasterInvalidationTrackingInfo {
    using ClientDebugNamesMap = HashMap<const DisplayItemClient*, String>;
    ClientDebugNamesMap new_client_debug_names;
    ClientDebugNamesMap old_client_debug_names;
    RasterInvalidationTracking tracking;
  };
  std::unique_ptr<RasterInvalidationTrackingInfo>
      raster_invalidation_tracking_info_;
};

}  // namespace blink

#endif  // ContentLayerClientImpl_h
