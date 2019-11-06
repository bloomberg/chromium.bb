// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/paint_worklet_image_cache.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "cc/paint/paint_worklet_layer_painter.h"

namespace cc {

class PaintWorkletTaskImpl : public TileTask {
 public:
  PaintWorkletTaskImpl(PaintWorkletImageCache* cache,
                       const PaintImage& paint_image)
      : TileTask(true), cache_(cache), paint_image_(paint_image) {}
  PaintWorkletTaskImpl(const PaintWorkletTaskImpl&) = delete;

  PaintWorkletTaskImpl& operator=(const PaintWorkletTaskImpl&) = delete;

  // Overridden from Task:
  void RunOnWorkerThread() override { cache_->PaintImageInTask(paint_image_); }

  // Overridden from TileTask:
  void OnTaskCompleted() override {}

 protected:
  ~PaintWorkletTaskImpl() override = default;

 private:
  PaintWorkletImageCache* cache_;
  PaintImage paint_image_;
};

PaintWorkletImageCache::PaintWorkletImageCache() {}

PaintWorkletImageCache::~PaintWorkletImageCache() {
  for (const auto& pair : records_)
    DCHECK_EQ(pair.second.used_ref_count, 0u);
}

void PaintWorkletImageCache::SetPaintWorkletLayerPainter(
    std::unique_ptr<PaintWorkletLayerPainter> painter) {
  DCHECK(!painter_);
  painter_ = std::move(painter);
}

scoped_refptr<TileTask> PaintWorkletImageCache::GetTaskForPaintWorkletImage(
    const DrawImage& image) {
  DCHECK(painter_);
  DCHECK(image.paint_image().IsPaintWorklet());
  return base::MakeRefCounted<PaintWorkletTaskImpl>(this, image.paint_image());
}

// TODO(xidachen): we might need to consider the animated property value and the
// PaintWorkletInput to decide whether we need to call Paint() function or not.
void PaintWorkletImageCache::PaintImageInTask(const PaintImage& paint_image) {
  // TODO(crbug.com/939009): When creating a TileTask for a given PaintImage at
  // GetTaskForPaintWorkletImage, we should not create a new TileTask if there
  // is already a TileTask for this PaintImage.
  {
    base::AutoLock hold(records_lock_);
    if (records_.find(paint_image.paint_worklet_input()) != records_.end())
      return;
  }
  // Because the compositor could be waiting on the lock in NotifyPrepareTiles,
  // we unlock here such that the compositor won't be blocked on potentially
  // slow Paint function.
  // TODO(xidachen): ensure that the canvas operations in the PaintRecord
  // matches the PaintGeneratedImage::Draw.
  sk_sp<PaintRecord> record =
      painter_->Paint(paint_image.paint_worklet_input());
  if (!record)
    return;
  {
    base::AutoLock hold(records_lock_);
    // It is possible for two or more threads to both pass through the first
    // lock and arrive here. To avoid ref-count issues caused by potential
    // racing among threads, we use insert such that if an entry already exists
    // for a particular key, the value won't be overridden.
    records_.insert(
        std::make_pair(paint_image.paint_worklet_input(),
                       PaintWorkletImageCacheValue(std::move(record), 0)));
  }
}

std::pair<sk_sp<PaintRecord>, base::OnceCallback<void()>>
PaintWorkletImageCache::GetPaintRecordAndRef(PaintWorkletInput* input) {
  base::AutoLock hold(records_lock_);
  DCHECK(records_.find(input) != records_.end());
  records_[input].used_ref_count++;
  records_[input].num_of_frames_not_accessed = 0u;
  // The PaintWorkletImageCache object lives as long as the LayerTreeHostImpl,
  // and that ensures that this pointer and the input will be alive when this
  // callback is executed.
  auto callback =
      base::BindOnce(&PaintWorkletImageCache::DecrementCacheRefCount,
                     base::Unretained(this), base::Unretained(input));
  return std::make_pair(records_[input].record, std::move(callback));
}

void PaintWorkletImageCache::SetNumOfFramesToPurgeCacheEntryForTest(
    size_t num) {
  num_of_frames_to_purge_cache_entry_ = num;
}

void PaintWorkletImageCache::DecrementCacheRefCount(PaintWorkletInput* input) {
  base::AutoLock hold(records_lock_);
  auto it = records_.find(input);
  DCHECK(it != records_.end());

  auto& pair = it->second;
  DCHECK_GT(pair.used_ref_count, 0u);
  pair.used_ref_count--;
}

void PaintWorkletImageCache::NotifyDidPrepareTiles() {
  base::AutoLock hold(records_lock_);
  base::EraseIf(
      records_,
      [this](
          const std::pair<PaintWorkletInput*, PaintWorkletImageCacheValue>& t) {
        return t.second.num_of_frames_not_accessed >=
                   num_of_frames_to_purge_cache_entry_ &&
               t.second.used_ref_count == 0;
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
