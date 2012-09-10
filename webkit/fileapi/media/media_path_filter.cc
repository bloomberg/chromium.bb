// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/media/media_path_filter.h"

#include <algorithm>
#include <string>

#include "base/string_util.h"
#include "net/base/mime_util.h"

namespace fileapi {

namespace {

bool IsUnsupportedExtension(const FilePath::StringType& extension) {
  std::string mime_type;
  return !net::GetMimeTypeFromExtension(extension, &mime_type) ||
      !net::IsSupportedMimeType(mime_type);
}

}  // namespace

MediaPathFilter::MediaPathFilter()
    : initialized_(false) {
}

MediaPathFilter::~MediaPathFilter() {
}

bool MediaPathFilter::Match(const FilePath& path) {
  EnsureInitialized();
  return std::binary_search(media_file_extensions_.begin(),
                            media_file_extensions_.end(),
                            StringToLowerASCII(path.Extension()));
}

void MediaPathFilter::EnsureInitialized() {
  if (initialized_)
    return;

  base::AutoLock lock(initialization_lock_);
  if (initialized_)
    return;

  net::GetExtensionsForMimeType("image/*", &media_file_extensions_);
  net::GetExtensionsForMimeType("audio/*", &media_file_extensions_);
  net::GetExtensionsForMimeType("video/*", &media_file_extensions_);

  MediaFileExtensionList::iterator new_end =
      std::remove_if(media_file_extensions_.begin(),
                     media_file_extensions_.end(),
                     &IsUnsupportedExtension);
  media_file_extensions_.erase(new_end, media_file_extensions_.end());

  for (MediaFileExtensionList::iterator itr = media_file_extensions_.begin();
       itr != media_file_extensions_.end(); ++itr)
    *itr = FilePath::kExtensionSeparator + *itr;
  std::sort(media_file_extensions_.begin(), media_file_extensions_.end());

  initialized_ = true;
}

}  // namespace fileapi
