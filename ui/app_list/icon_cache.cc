// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/icon_cache.h"

#include "base/logging.h"
#include "base/md5.h"
#include "ui/gfx/size.h"

namespace {

// Gets cache key based on |image| contents and desired |size|.
std::string GetKey(const SkBitmap& image, const gfx::Size& size) {
  SkAutoLockPixels image_lock(image);
  base::MD5Digest digest;
  MD5Sum(image.getPixels(), image.getSize(), &digest);

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

bool IconCache::Get(const SkBitmap& src,
                    const gfx::Size& size,
                    SkBitmap* processed) {
  Cache::iterator it = cache_.find(GetKey(src, size));
  if (it == cache_.end())
    return false;

  it->second.used = true;

  if (processed)
    *processed = it->second.image;
  return true;
}

void IconCache::Put(const SkBitmap& src,
                    const gfx::Size& size,
                    const SkBitmap& processed) {
  const std::string key = GetKey(src, size);
  cache_[key].image = processed;
  cache_[key].used = true;
}

IconCache::IconCache() {
}

IconCache::~IconCache() {
}

}  // namespace app_list
