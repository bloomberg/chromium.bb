// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "webkit/fileapi/media/filtering_file_enumerator.h"

#if defined(OS_WIN)
#include <windows.h>
#else
#include <cstring>

#include "base/string_util.h"
#endif

namespace fileapi {

namespace {

// Used to skip the hidden folders and files. Returns true if the file specified
// by |path| should be skipped.
bool ShouldSkip(const base::FilePath& path) {
  const base::FilePath& base_name = path.BaseName();
  if (base_name.empty())
    return false;

  // Dot files (aka hidden files)
  if (base_name.value()[0] == '.')
    return true;

  // Mac OS X file.
  if (base_name.value() == FILE_PATH_LITERAL("__MACOSX"))
    return true;

#if defined(OS_WIN)
  DWORD file_attributes = ::GetFileAttributes(path.value().c_str());
  if ((file_attributes != INVALID_FILE_ATTRIBUTES) &&
      ((file_attributes & FILE_ATTRIBUTE_HIDDEN) != 0))
    return true;
#else
  // Windows always creates a recycle bin folder in the attached device to store
  // all the deleted contents. On non-windows operating systems, there is no way
  // to get the hidden attribute of windows recycle bin folders that are present
  // on the attached device. Therefore, compare the file path name to the
  // recycle bin name and exclude those folders. For more details, please refer
  // to http://support.microsoft.com/kb/171694.
  const char win_98_recycle_bin_name[] = "RECYCLED";
  const char win_xp_recycle_bin_name[] = "RECYCLER";
  const char win_vista_recycle_bin_name[] = "$Recycle.bin";
  if ((base::strncasecmp(base_name.value().c_str(),
                         win_98_recycle_bin_name,
                         strlen(win_98_recycle_bin_name)) == 0) ||
      (base::strncasecmp(base_name.value().c_str(),
                         win_xp_recycle_bin_name,
                         strlen(win_xp_recycle_bin_name)) == 0) ||
      (base::strncasecmp(base_name.value().c_str(),
                         win_vista_recycle_bin_name,
                         strlen(win_vista_recycle_bin_name)) == 0))
    return true;
#endif
  return false;
}

}  // namespace

FilteringFileEnumerator::FilteringFileEnumerator(
    scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator> base_enumerator,
    MediaPathFilter* filter)
    : base_enumerator_(base_enumerator.Pass()),
      filter_(filter) {
  DCHECK(base_enumerator_.get());
  DCHECK(filter);
}

FilteringFileEnumerator::~FilteringFileEnumerator() {
}

base::FilePath FilteringFileEnumerator::Next() {
  while (true) {
    base::FilePath next = base_enumerator_->Next();
    if (ShouldSkip(next))
      continue;

    if (next.empty() ||
        base_enumerator_->IsDirectory() ||
        filter_->Match(next))
      return next;
  }
}

int64 FilteringFileEnumerator::Size() {
  return base_enumerator_->Size();
}

base::Time FilteringFileEnumerator::LastModifiedTime() {
  return base_enumerator_->LastModifiedTime();
}

bool FilteringFileEnumerator::IsDirectory() {
  return base_enumerator_->IsDirectory();
}

}  // namespace fileapi
