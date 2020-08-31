// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/memory/pagemap.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string>

#include "base/memory/aligned_memory.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/process_metrics.h"
#include "base/strings/stringprintf.h"
#include "base/threading/scoped_blocking_call.h"

namespace chromeos {
namespace memory {

namespace {
constexpr char kPagemapFileFormat[] = "/proc/%d/pagemap";
}

Pagemap::~Pagemap() = default;

Pagemap::Pagemap(pid_t pid) {
  if (pid) {
    std::string pagemap_file = base::StringPrintf(kPagemapFileFormat, pid);
    fd_.reset(HANDLE_EINTR(open(pagemap_file.c_str(), O_RDONLY)));
  }
}

bool Pagemap::IsValid() const {
  return fd_.is_valid();
}

bool Pagemap::GetEntries(uint64_t address,
                         uint64_t length,
                         std::vector<PagemapEntry>* entries) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  DCHECK(IsValid());
  CHECK(entries);

  const size_t kPageSize = base::GetPageSize();
  CHECK(base::IsPageAligned(address));
  CHECK(base::IsPageAligned(length));

  // The size of each pagemap entry to calculate our offset in the file.
  uint64_t num_pages = length / kPageSize;

  if (entries->size() != num_pages) {
    // Shrink or grow entries to the correct length if it was not already.
    entries->resize(num_pages);
    entries->shrink_to_fit();  // If we made it smaller shrink capacity.
  }

  uint64_t pagemap_offset = (address / kPageSize) * sizeof(PagemapEntry);
  uint64_t pagemap_len = num_pages * sizeof(PagemapEntry);

  memset(entries->data(), 0, pagemap_len);

  // The caller was expected to provide a buffer large enough for the number of
  // pages in the region.
  uint64_t total_read = 0;
  while (total_read < pagemap_len) {
    ssize_t bytes_read = HANDLE_EINTR(
        pread(fd_.get(), reinterpret_cast<char*>(entries->data()) + total_read,
              pagemap_len - total_read, pagemap_offset + total_read));

    if (bytes_read <= 0) {
      return false;
    }
    total_read += bytes_read;
  }

  return true;
}

}  // namespace memory
}  // namespace chromeos
