// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_AMBIENT_PHOTO_CACHE_H_
#define ASH_AMBIENT_AMBIENT_PHOTO_CACHE_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/ambient/proto/photo_cache_entry.pb.h"
#include "base/callback_forward.h"
#include "base/files/file_path.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {

// Interface for downloading and decoding photos for Ambient mode. Mocked for
// testing to isolate from network and file system.
// Each cache entry is written to disk as three files in the |root_path|, with
// filenames prefixed by |cache_index|.
class ASH_EXPORT AmbientPhotoCache {
 public:
  AmbientPhotoCache() = default;
  AmbientPhotoCache(const AmbientPhotoCache&) = delete;
  AmbientPhotoCache& operator=(const AmbientPhotoCache&) = delete;
  virtual ~AmbientPhotoCache() = default;

  static std::unique_ptr<AmbientPhotoCache> Create(base::FilePath root_path);

  virtual void DownloadPhoto(
      const std::string& url,
      base::OnceCallback<void(std::string&&)> callback) = 0;

  // Saves the photo at |url| to |cache_index| and calls |callback| with a
  // boolean that indicates success.
  virtual void DownloadPhotoToFile(const std::string& url,
                                   int cache_index,
                                   base::OnceCallback<void(bool)> callback) = 0;

  virtual void DecodePhoto(
      const std::string& data,
      base::OnceCallback<void(const gfx::ImageSkia&)> callback) = 0;

  // Write photo cache to disk at |cache_index| and call |callback| when
  // complete.
  virtual void WritePhotoCache(int cache_index,
                               const ambient::PhotoCacheEntry& cache_entry,
                               base::OnceClosure callback) = 0;

  // Read the photo cache at |cache_index| and call |callback| when complete.
  // If a particular cache fails to be read, |cache_entry| will be empty.
  virtual void ReadPhotoCache(int cache_index,
                              ambient::PhotoCacheEntry* cache_entry,
                              base::OnceCallback<void()> callback) = 0;

  // Erase all stored files from disk.
  virtual void Clear() = 0;
};

}  // namespace ash

#endif  // ASH_AMBIENT_AMBIENT_PHOTO_CACHE_H_
