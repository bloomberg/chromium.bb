// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/icon_cache.h"

#include <algorithm>

#include "base/logging.h"
#include "base/md5.h"
#include "ui/gfx/size.h"

namespace {

// Predicator for sorting ImageSkiaRep by scale factor.
bool ImageRepScaleFactorCompare(const gfx::ImageSkiaRep& rep1,
                                const gfx::ImageSkiaRep& rep2) {
  return rep1.scale_factor() < rep2.scale_factor();
}

// Gets cache key based on |image| contents and desired |size|.
std::string GetKey(const gfx::ImageSkia& image, const gfx::Size& size) {
  gfx::ImageSkia::ImageSkiaReps image_reps = image.image_reps();
  DCHECK_GT(image_reps.size(), 0u);

  std::sort(image_reps.begin(), image_reps.end(), &ImageRepScaleFactorCompare);

  base::MD5Context ctx;
  base::MD5Init(&ctx);
  for (gfx::ImageSkia::ImageSkiaReps::const_iterator it = image_reps.begin();
       it != image_reps.end(); ++it) {
    const SkBitmap& bitmap = it->sk_bitmap();
    SkAutoLockPixels image_lock(bitmap);

    base::MD5Update(
        &ctx,
        base::StringPiece(reinterpret_cast<const char*>(bitmap.getPixels()),
                          bitmap.getSize()));
  }

  base::MD5Digest digest;
  base::MD5Final(&digest, &ctx);

  return MD5DigestToBase16(digest) + "." + size.ToString();
}

}  // namespace

namespace app_list {

// static
IconCache* IconCache::instance_ = NULL;

// static
void IconCache::CreateInstance() {
  DCHECK(!instance_);
  instance_ = new IconCache;
}

// static
void IconCache::DeleteInstance() {
  DCHECK(instance_);
  delete instance_;
  instance_ = NULL;
}

// static
IconCache* IconCache::GetInstance() {
  DCHECK(instance_);
  return instance_;
}

void IconCache::MarkAllEntryUnused() {
  for (Cache::iterator i = cache_.begin(); i != cache_.end(); ++i)
    i->second.used = false;
}

void IconCache::PurgeAllUnused() {
  for (Cache::iterator i = cache_.begin(); i != cache_.end();) {
    Cache::iterator current(i);
    ++i;
    if (!current->second.used)
      cache_.erase(current);
  }
}

bool IconCache::Get(const gfx::ImageSkia& src,
                    const gfx::Size& size,
                    gfx::ImageSkia* processed) {
  Cache::iterator it = cache_.find(GetKey(src, size));
  if (it == cache_.end())
    return false;

  it->second.used = true;

  if (processed)
    *processed = it->second.image;
  return true;
}

void IconCache::Put(const gfx::ImageSkia& src,
                    const gfx::Size& size,
                    const gfx::ImageSkia& processed) {
  const std::string key = GetKey(src, size);
  cache_[key].image = processed;
  cache_[key].used = true;
}

IconCache::IconCache() {
}

IconCache::~IconCache() {
}

}  // namespace app_list
