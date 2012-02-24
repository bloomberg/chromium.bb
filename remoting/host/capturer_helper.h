// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CAPTURER_HELPER_H_
#define REMOTING_HOST_CAPTURER_HELPER_H_

#include "base/synchronization/lock.h"
#include "third_party/skia/include/core/SkRegion.h"

namespace remoting {

// CapturerHelper is intended to be used by an implementation of the Capturer
// interface. It maintains a thread-safe invalid region, and the size of the
// most recently captured screen, on behalf of the Capturer that owns it.
class CapturerHelper {
 public:
  CapturerHelper();
  ~CapturerHelper();

  // Clear out the invalid region.
  void ClearInvalidRegion();

  // Invalidate the specified region.
  void InvalidateRegion(const SkRegion& invalid_region);

  // Invalidate the entire screen, of a given size.
  void InvalidateScreen(const SkISize& size);

  // Invalidate the entire screen, using the size of the most recently
  // captured screen.
  void InvalidateFullScreen();

  // Swap the given region with the stored invalid region.
  void SwapInvalidRegion(SkRegion* invalid_region);

  // Access the size of the most recently captured screen.
  const SkISize& size_most_recent() const;
  void set_size_most_recent(const SkISize& size);

 private:
  // A region that has been manually invalidated (through InvalidateRegion).
  // These will be returned as dirty_region in the capture data during the next
  // capture.
  SkRegion invalid_region_;

  // A lock protecting |invalid_region_| across threads.
  base::Lock invalid_region_lock_;

  // The size of the most recently captured screen.
  SkISize size_most_recent_;

  DISALLOW_COPY_AND_ASSIGN(CapturerHelper);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CAPTURER_HELPER_H_
