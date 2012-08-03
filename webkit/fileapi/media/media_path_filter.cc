// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/media/media_path_filter.h"

#include <algorithm>
#include <string>

#include "base/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/mime_util.h"

namespace fileapi {

namespace {

bool IsUnsupportedExtension(const FilePath::StringType& extension) {
  std::string mime_type;
  return !net::GetMimeTypeFromExtension(extension, &mime_type) ||
      !net::IsSupportedMimeType(mime_type);
}

}  // namespace

MediaPathFilter::MediaPathFilter() {
  // TODO(tzik): http://crbug.com/140401
  // Remove this ScopedAllowIO after move this to FILE thread.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  net::GetImageExtensions(&media_file_extensions_);
  net::GetAudioExtensions(&media_file_extensions_);
  net::GetVideoExtensions(&media_file_extensions_);

  MediaFileExtensionList::iterator new_end =
      std::remove_if(media_file_extensions_.begin(),
                     media_file_extensions_.end(),
                     &IsUnsupportedExtension);
  media_file_extensions_.erase(new_end, media_file_extensions_.end());

  for (MediaFileExtensionList::iterator itr = media_file_extensions_.begin();
       itr != media_file_extensions_.end(); ++itr)
    *itr = FilePath::kExtensionSeparator + *itr;
  std::sort(media_file_extensions_.begin(), media_file_extensions_.end());
}

MediaPathFilter::~MediaPathFilter() {
}

bool MediaPathFilter::Match(const FilePath& path) const {
  return std::binary_search(media_file_extensions_.begin(),
                            media_file_extensions_.end(),
                            StringToLowerASCII(path.Extension()));
}

}  // namespace fileapi
