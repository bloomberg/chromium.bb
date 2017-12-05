// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OpenedFrameTracker_h
#define OpenedFrameTracker_h

#include "base/macros.h"
#include "platform/wtf/HashSet.h"

namespace blink {

class WebFrame;

// Small helper class to track the set of frames that a WebFrame has opened.
// Due to layering restrictions, we need to hide the implementation, since
// public/web/ cannot depend on wtf/.
class OpenedFrameTracker {
 public:
  OpenedFrameTracker();
  ~OpenedFrameTracker();

  bool IsEmpty() const;
  void Add(WebFrame*);
  void Remove(WebFrame*);

  // Helper used when swapping a frame into the frame tree: this updates the
  // opener for opened frames to point to the new frame being swapped in.
  void TransferTo(WebFrame*);

  // Helper function to clear the openers when the frame is being detached.
  void Dispose() { TransferTo(nullptr); }

 private:
  WTF::HashSet<WebFrame*> opened_frames_;

  DISALLOW_COPY_AND_ASSIGN(OpenedFrameTracker);
};

}  // namespace blink

#endif  // WebFramePrivate_h
