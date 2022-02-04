// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_OP_BUFFER_SERIALIZER_H_
#define CC_PAINT_PAINT_OP_BUFFER_SERIALIZER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "cc/paint/paint_op_buffer.h"

#include "third_party/skia/include/private/chromium/SkChromeRemoteGlyphCache.h"
#include "ui/gfx/geometry/rect_f.h"

namespace cc {

class CC_PAINT_EXPORT PaintOpBufferSerializer {
 public:
  using SerializeCallback =
      base::RepeatingCallback<size_t(const PaintOp*,
                                     const PaintOp::SerializeOptions&,
                                     const PaintFlags*,
                                     const SkM44&,
                                     const SkM44&)>;

  PaintOpBufferSerializer(SerializeCallback serialize_cb,
                          const PaintOp::SerializeOptions& options);
  virtual ~PaintOpBufferSerializer();

  struct Preamble {
    // The full size of the content, to know whether edge texel clearing
    // is required or not.  The full_raster_rect and playback_rect must
    // be contained in this size.
    gfx::Size content_size;
    // Rect in content space (1 unit = 1 pixel) of this tile.
    gfx::Rect full_raster_rect;
    // A subrect in content space (full_raster_rect must contain this) of
    // the partial content to play back.
    gfx::Rect playback_rect;
    // The translation and scale to do after
    gfx::Vector2dF post_translation;
    gfx::Vector2dF post_scale = gfx::Vector2dF(1.f, 1.f);
    // If requires_clear is true, then this will raster will be cleared to
    // transparent.  If false, it assumes that the content will raster
    // opaquely up to content_size inset by 1 (with the last pixel being
    // potentially being partially transparent, if post scaled).
    bool requires_clear = true;
    // If clearing is needed, the color to clear to.
    SkColor background_color = SK_ColorTRANSPARENT;
  };
  // Serialize the buffer with a preamble. This function wraps the buffer in a
  // save/restore and includes any translations, scales, and clearing as
  // specified by the preamble.  This should generally be used for top level
  // rastering of an entire tile.
  void Serialize(const PaintOpBuffer* buffer,
                 const std::vector<size_t>* offsets,
                 const Preamble& preamble);
  // Sereialize the buffer as |Serialize| with a preamble. This function also
  // destroys the PaintOps in |buffer| after serialization.
  void SerializeAndDestroy(PaintOpBuffer* buffer,
                           const std::vector<size_t>* offsets,
                           const Preamble& preamble);
  // Serialize the buffer without a preamble. This function serializes the whole
  // buffer without any extra ops added.  No clearing is done.  This should
  // generally be used for internal PaintOpBuffers that want to be sent as-is.
  void Serialize(const PaintOpBuffer* buffer);
  // Serialize the buffer with a scale and a playback rect.  This should
  // generally be used for internal PaintOpBuffers in PaintShaders and
  // PaintFilters that need to guarantee the nested buffer is rasterized at the
  // specific scale to a separate image. This ensures that scale-dependent
  // analysis made during serialization is consistent with analysis done during
  // rasterization.
  void Serialize(const PaintOpBuffer* buffer,
                 const gfx::Rect& playback_rect,
                 const gfx::SizeF& post_scale);

  bool valid() const { return valid_; }

 private:
  void SerializePreamble(SkCanvas* canvas,
                         const Preamble& preamble,
                         const PlaybackParams& params);
  void SerializeBuffer(SkCanvas* canvas,
                       const PaintOpBuffer* buffer,
                       const std::vector<size_t>* offsets);
  void SerializeBufferAndDestroy(SkCanvas* canvas,
                                 PaintOpBuffer* buffer,
                                 const std::vector<size_t>* offsets);
  // Returns whether searilization of |op| succeeded and we need to serialize
  // the next PaintOp in the PaintOpBuffer.
  bool WillSerializeNextOp(const PaintOp* op,
                           SkCanvas* canvas,
                           PlaybackParams params,
                           uint8_t alpha);
  bool SerializeOpWithFlags(SkCanvas* canvas,
                            const PaintOpWithFlags* flags_op,
                            const PlaybackParams& params,
                            uint8_t alpha);
  bool SerializeOp(SkCanvas* canvas,
                   const PaintOp* op,
                   const PaintFlags* flags_to_serialize,
                   const PlaybackParams& params);
  void Save(SkCanvas* canvas, const PlaybackParams& params);
  void RestoreToCount(SkCanvas* canvas,
                      int count,
                      const PlaybackParams& params);
  void ClearForOpaqueRaster(SkCanvas* canvas,
                            const Preamble& preamble,
                            const PlaybackParams& params);
  void PlaybackOnAnalysisCanvas(SkCanvas* canvas,
                                const PaintOp* op,
                                const PaintFlags* flags_to_serialize,
                                const PlaybackParams& params);

  SerializeCallback serialize_cb_;
  PaintOp::SerializeOptions options_;

  bool valid_ = true;
};

// Serializes the ops in the memory available, fails on overflow.
class CC_PAINT_EXPORT SimpleBufferSerializer : public PaintOpBufferSerializer {
 public:
  SimpleBufferSerializer(void* memory,
                         size_t size,
                         const PaintOp::SerializeOptions& options);
  ~SimpleBufferSerializer() override;

  size_t written() const { return written_; }

 private:
  size_t SerializeToMemory(const PaintOp* op,
                           const PaintOp::SerializeOptions& options,
                           const PaintFlags* flags_to_serialize,
                           const SkM44& current_ctm,
                           const SkM44& original_ctm);

  raw_ptr<void> memory_;
  const size_t total_;
  size_t written_ = 0u;
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_OP_BUFFER_SERIALIZER_H_
