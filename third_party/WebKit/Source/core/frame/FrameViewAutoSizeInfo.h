// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FrameViewAutoSizeInfo_h
#define FrameViewAutoSizeInfo_h

#include "platform/geometry/IntSize.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

class LocalFrameView;

class FrameViewAutoSizeInfo final
    : public GarbageCollected<FrameViewAutoSizeInfo> {
  WTF_MAKE_NONCOPYABLE(FrameViewAutoSizeInfo);

 public:
  static FrameViewAutoSizeInfo* Create(LocalFrameView* frame_view) {
    return new FrameViewAutoSizeInfo(frame_view);
  }

  void ConfigureAutoSizeMode(const IntSize& min_size, const IntSize& max_size);
  void AutoSizeIfNeeded();

  void Trace(blink::Visitor*);

 private:
  explicit FrameViewAutoSizeInfo(LocalFrameView*);

  Member<LocalFrameView> frame_view_;

  // The lower bound on the size when autosizing.
  IntSize min_auto_size_;
  // The upper bound on the size when autosizing.
  IntSize max_auto_size_;

  bool in_auto_size_;
  // True if autosize has been run since m_shouldAutoSize was set.
  bool did_run_autosize_;
};

}  // namespace blink

#endif  // FrameViewAutoSizeInfo_h
