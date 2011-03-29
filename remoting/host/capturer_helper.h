// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CAPTURER_HELPER_H_
#define REMOTING_HOST_CAPTURER_HELPER_H_

#include "base/synchronization/lock.h"
#include "remoting/base/types.h"

namespace remoting {

// CapturerHelper is intended to be used by an implementation of the Capturer
// interface. It maintains a thread-safe list of invalid rectangles, and the
// size of the most recently captured screen, on behalf of the Capturer that
// owns it.
class CapturerHelper {
 public:
  CapturerHelper();
  ~CapturerHelper();

  // Clear out the list of invalid rects.
  void ClearInvalidRects();

  // Invalidate the specified screen rects.
  void InvalidateRects(const InvalidRects& inval_rects);

  // Invalidate the entire screen, of a given size.
  void InvalidateScreen(const gfx::Size& size);

  // Invalidate the entire screen, using the size of the most recently
  // captured screen.
  void InvalidateFullScreen();

  // Whether the invalid region is a full screen of a given size.
  bool IsCaptureFullScreen(const gfx::Size& size);

  // Swap the given set of rects with the stored invalid rects.
  // This should be used like this:
  //
  // InvalidRects inval_rects;
  // common.SwapInvalidRects(inval_rects);
  //
  // This passes the invalid rects to the caller, and removes them from this
  // object. The caller should then pass the raster data in those rects to the
  // client.
  void SwapInvalidRects(InvalidRects& inval_rects);

  // Access the size of the most recently captured screen.
  const gfx::Size& size_most_recent() const;
  void set_size_most_recent(const gfx::Size& size);

 private:
  // Rects that have been manually invalidated (through InvalidateRect).
  // These will be returned as dirty_rects in the capture data during the next
  // capture.
  InvalidRects inval_rects_;

  // A lock protecting |inval_rects_| across threads.
  base::Lock inval_rects_lock_;

  // The size of the most recently captured screen.
  gfx::Size size_most_recent_;

  DISALLOW_COPY_AND_ASSIGN(CapturerHelper);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CAPTURER_HELPER_H_
