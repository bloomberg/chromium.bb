// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_MEMORY_MANAGED_PAINT_CANVAS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_MEMORY_MANAGED_PAINT_CANVAS_H_

#include <memory>

#include "cc/paint/record_paint_canvas.h"
#include "third_party/blink/public/platform/platform.h"

namespace blink {

// MemoryManagedPaintCanvas overrides the potentially memory intensive image
// drawing methods of PaintCanvas and keeps track of how much memory is
// being pinned between flushes. This allows the rendering context to flush if
// too much memory is used.
class PLATFORM_EXPORT MemoryManagedPaintCanvas final
    : public cc::RecordPaintCanvas {
 public:
  MemoryManagedPaintCanvas(cc::DisplayItemList* list,
                           const SkRect& bounds,
                           base::RepeatingClosure set_needs_flush_callback);
  explicit MemoryManagedPaintCanvas(const cc::RecordPaintCanvas&) = delete;
  ~MemoryManagedPaintCanvas() override;

  void drawImage(const cc::PaintImage& image,
                 SkScalar left,
                 SkScalar top,
                 const cc::PaintFlags* flags) override;
  void drawImageRect(const cc::PaintImage& image,
                     const SkRect& src,
                     const SkRect& dst,
                     const cc::PaintFlags* flags,
                     SrcRectConstraint constraint) override;

 private:
  void UpdateMemoryUsage(const cc::PaintImage& image);

  base::flat_set<int> cached_image_ids_;
  uint64_t total_stored_image_memory_ = 0;

  base::RepeatingClosure set_needs_flush_callback_;

  // The same value as is used in content::WebGraphicsConext3DProviderImpl.
  static constexpr uint64_t kMaxPinnedMemory = 64 * 1024 * 1024;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_MEMORY_MANAGED_PAINT_CANVAS_H_
