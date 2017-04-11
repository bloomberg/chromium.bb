// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClipList_h
#define ClipList_h

#include "platform/graphics/GraphicsTypes.h"
#include "platform/graphics/paint/PaintCanvas.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Vector.h"

class SkPath;

namespace blink {

class ClipList {
  DISALLOW_NEW();

 public:
  ClipList() {}
  ClipList(const ClipList&);
  ~ClipList() {}

  void ClipPath(const SkPath&, AntiAliasingMode, const SkMatrix&);
  void Playback(PaintCanvas*) const;
  const SkPath& GetCurrentClipPath() const;

 private:
  struct ClipOp {
    SkPath path_;
    AntiAliasingMode anti_aliasing_mode_;

    ClipOp();
    ClipOp(const ClipOp&);
  };

  // Number of clip ops that can be stored in a ClipList without resorting to
  // dynamic allocation
  static const size_t kCInlineClipOpCapacity = 4;

  WTF::Vector<ClipOp, kCInlineClipOpCapacity> clip_list_;
  SkPath current_clip_path_;
};

}  // namespace blink

#endif
