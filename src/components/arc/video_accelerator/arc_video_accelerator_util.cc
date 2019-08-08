// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/video_accelerator/arc_video_accelerator_util.h"

#include <sys/types.h>
#include <unistd.h>

#include "base/files/platform_file.h"
#include "media/gpu/macros.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace arc {

base::ScopedFD UnwrapFdFromMojoHandle(mojo::ScopedHandle handle) {
  if (!handle.is_valid()) {
    VLOGF(1) << "Handle is invalid.";
    return base::ScopedFD();
  }

  base::PlatformFile platform_file;
  MojoResult mojo_result =
      mojo::UnwrapPlatformFile(std::move(handle), &platform_file);
  if (mojo_result != MOJO_RESULT_OK) {
    VLOGF(1) << "UnwrapPlatformFile failed: " << mojo_result;
    return base::ScopedFD();
  }

  return base::ScopedFD(platform_file);
}

bool GetFileSize(const int fd, size_t* size) {
  off_t fd_size = lseek(fd, 0, SEEK_END);
  lseek(fd, 0, SEEK_SET);
  if (fd_size < 0u) {
    VPLOGF(1) << "Fail to find the size of fd.";
    return false;
  }

  if (!base::IsValueInRangeForNumericType<size_t>(fd_size)) {
    VLOGF(1) << "fd_size is out of range of size_t"
             << ", size=" << size
             << ", size_t max=" << std::numeric_limits<size_t>::max()
             << ", size_t min=" << std::numeric_limits<size_t>::min();
    return false;
  }

  *size = static_cast<size_t>(fd_size);
  return true;
}

}  // namespace arc
