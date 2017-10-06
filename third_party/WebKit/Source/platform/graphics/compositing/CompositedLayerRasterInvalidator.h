// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositedLayerRasterInvalidator_h
#define CompositedLayerRasterInvalidator_h

#include "platform/graphics/paint/PaintChunk.h"
#include "platform/graphics/paint/RasterInvalidationTracking.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/Vector.h"

namespace blink {

class IntRect;

class PLATFORM_EXPORT CompositedLayerRasterInvalidator {
 public:
  using RasterInvalidationFunction = std::function<void(const IntRect&)>;

  CompositedLayerRasterInvalidator(
      RasterInvalidationFunction raster_invalidation_function)
      : raster_invalidation_function_(raster_invalidation_function),
        layer_state_(nullptr, nullptr, nullptr) {}

  void SetTracksRasterInvalidations(bool);
  RasterInvalidationTracking* GetTracking() const {
    return tracking_info_ ? &tracking_info_->tracking : nullptr;
  }

  RasterInvalidationTracking& EnsureTracking();

  void Generate(const gfx::Rect& layer_bounds,
                const Vector<const PaintChunk*>&,
                const PropertyTreeState&);

  bool Matches(const PaintChunk& paint_chunk) const {
    return paint_chunks_info_.size() && paint_chunks_info_[0].is_cacheable &&
           paint_chunk.Matches(paint_chunks_info_[0].id);
  }

  const PropertyTreeState& GetLayerState() const { return layer_state_; }
  const gfx::Rect& LayerBounds() const { return layer_bounds_; }

 private:
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

  IntRect MapRectFromChunkToLayer(const FloatRect&, const PaintChunk&) const;

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

  RasterInvalidationFunction raster_invalidation_function_;
  gfx::Rect layer_bounds_;
  PropertyTreeState layer_state_;
  Vector<PaintChunkInfo> paint_chunks_info_;

  struct RasterInvalidationTrackingInfo {
    using ClientDebugNamesMap = HashMap<const DisplayItemClient*, String>;
    ClientDebugNamesMap new_client_debug_names;
    ClientDebugNamesMap old_client_debug_names;
    RasterInvalidationTracking tracking;
  };
  std::unique_ptr<RasterInvalidationTrackingInfo> tracking_info_;
};

}  // namespace blink

#endif  // CompositedLayerRasterInvalidator_h
