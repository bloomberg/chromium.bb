// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/paint_worklet_image_cache.h"

#include "base/bind.h"
#include "cc/paint/paint_worklet_layer_painter.h"

namespace cc {

class PaintWorkletTaskImpl : public TileTask {
 public:
  PaintWorkletTaskImpl(PaintWorkletImageCache* cache,
                       const PaintImage& paint_image)
      : TileTask(true), cache_(cache), paint_image_(paint_image) {}

  // Overridden from Task:
  void RunOnWorkerThread() override { cache_->PaintImageInTask(paint_image_); }

  // Overridden from TileTask:
  void OnTaskCompleted() override {}

 protected:
  ~PaintWorkletTaskImpl() override = default;

 private:
  PaintWorkletImageCache* cache_;
  PaintImage paint_image_;

  DISALLOW_COPY_AND_ASSIGN(PaintWorkletTaskImpl);
};

PaintWorkletImageCache::PaintWorkletImageCache() {}

PaintWorkletImageCache::~PaintWorkletImageCache() {
  for (const auto& pair : records_)
    DCHECK_EQ(pair.second.used_ref_count, 0u);
}

void PaintWorkletImageCache::SetPaintWorkletLayerPainter(
    std::unique_ptr<PaintWorkletLayerPainter> painter) {
  painter_ = std::move(painter);
}

scoped_refptr<TileTask> PaintWorkletImageCache::GetTaskForPaintWorkletImage(
    const DrawImage& image) {
  return base::MakeRefCounted<PaintWorkletTaskImpl>(this, image.paint_image());
}

// TODO(xidachen): dispatch the work to a worklet thread, invoke JS callback.
// Do check the cache first. If there is already a cache entry for this input,
// then there is no need to call the Paint() function.
void PaintWorkletImageCache::PaintImageInTask(const PaintImage& paint_image) {
  // TODO(xidachen): ensure that the canvas operations in the PaintRecord
  // matches the PaintGeneratedImage::Draw.
  sk_sp<PaintRecord> record = painter_->Paint();
  records_[paint_image.paint_worklet_input()] =
      PaintWorkletImageCacheValue(std::move(record), 0);
}

std::pair<PaintRecord*, base::OnceCallback<void()>>
PaintWorkletImageCache::GetPaintRecordAndRef(PaintWorkletInput* input) {
  records_[input].used_ref_count++;
  records_[input].num_of_frames_not_accessed = 0u;
  // The PaintWorkletImageCache object lives as long as the LayerTreeHostImpl,
  // and that ensures that this pointer and the input will be alive when this
  // callback is executed.
  auto callback =
      base::BindOnce(&PaintWorkletImageCache::DecrementCacheRefCount,
                     base::Unretained(this), base::Unretained(input));
  return std::make_pair(records_[input].record.get(), std::move(callback));
}

void PaintWorkletImageCache::SetNumOfFramesToPurgeCacheEntryForTest(
    size_t num) {
  num_of_frames_to_purge_cache_entry_ = num;
}

void PaintWorkletImageCache::DecrementCacheRefCount(PaintWorkletInput* input) {
  auto it = records_.find(input);
  DCHECK(it != records_.end());

  auto& pair = it->second;
  DCHECK_GT(pair.used_ref_count, 0u);
  pair.used_ref_count--;
}

void PaintWorkletImageCache::NotifyDidPrepareTiles() {
  base::EraseIf(
      records_,
      [this](
          const std::pair<PaintWorkletInput*, PaintWorkletImageCacheValue>& t) {
        return t.second.num_of_frames_not_accessed >=
               num_of_frames_to_purge_cache_entry_;
      });
  for (auto& pair : records_)
    pair.second.num_of_frames_not_accessed++;
}

PaintWorkletImageCache::PaintWorkletImageCacheValue::
    PaintWorkletImageCacheValue() = default;

PaintWorkletImageCache::PaintWorkletImageCacheValue::
    PaintWorkletImageCacheValue(sk_sp<PaintRecord> record, size_t ref_count)
    : record(std::move(record)), used_ref_count(ref_count) {}

PaintWorkletImageCache::PaintWorkletImageCacheValue::
    PaintWorkletImageCacheValue(const PaintWorkletImageCacheValue& other)
    : record(other.record), used_ref_count(other.used_ref_count) {}

PaintWorkletImageCache::PaintWorkletImageCacheValue::
    ~PaintWorkletImageCacheValue() = default;

}  // namespace cc
