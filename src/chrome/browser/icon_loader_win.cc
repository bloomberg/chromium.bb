// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/icon_loader.h"

#include <windows.h>
#include <shellapi.h>

#include "base/bind.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/display/win/dpi.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/icon_util.h"
#include "ui/gfx/image/image_skia.h"

// static
IconLoader::IconGroup IconLoader::GroupForFilepath(
    const base::FilePath& file_path) {
  if (file_path.MatchesExtension(L".exe") ||
      file_path.MatchesExtension(L".dll") ||
      file_path.MatchesExtension(L".ico")) {
    return file_path.value();
  }

  return file_path.Extension();
}

// static
scoped_refptr<base::TaskRunner> IconLoader::GetReadIconTaskRunner() {
  // Technically speaking, only a thread with COM is needed, not one that has
  // a COM STA. However, this is what is available for now.
  return base::ThreadPool::CreateCOMSTATaskRunner(traits());
}

void IconLoader::ReadIcon() {
  int size = 0;
  switch (icon_size_) {
    case IconLoader::SMALL:
      size = SHGFI_SMALLICON;
      break;
    case IconLoader::NORMAL:
      size = 0;
      break;
    case IconLoader::LARGE:
      size = SHGFI_LARGEICON;
      break;
    default:
      NOTREACHED();
  }

  gfx::Image image;

  SHFILEINFO file_info = { 0 };
  if (SHGetFileInfo(group_.c_str(), FILE_ATTRIBUTE_NORMAL, &file_info,
                     sizeof(SHFILEINFO),
                     SHGFI_ICON | size | SHGFI_USEFILEATTRIBUTES)) {
    const SkBitmap bitmap = IconUtil::CreateSkBitmapFromHICON(file_info.hIcon);
    if (!bitmap.isNull()) {
      gfx::ImageSkia image_skia(
          gfx::ImageSkiaRep(bitmap, display::win::GetDPIScale()));
      image_skia.MakeThreadSafe();
      image = gfx::Image(image_skia);
      DestroyIcon(file_info.hIcon);
    }
  }

  target_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback_), std::move(image), group_));
  delete this;
}
