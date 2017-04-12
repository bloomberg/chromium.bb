// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VideoPainter_h
#define VideoPainter_h

#include "platform/wtf/Allocator.h"

namespace blink {

struct PaintInfo;
class LayoutPoint;
class LayoutVideo;

class VideoPainter {
  STACK_ALLOCATED();

 public:
  VideoPainter(const LayoutVideo& layout_video) : layout_video_(layout_video) {}

  void PaintReplaced(const PaintInfo&, const LayoutPoint&);

 private:
  const LayoutVideo& layout_video_;
};

}  // namespace blink

#endif  // VideoPainter_h
