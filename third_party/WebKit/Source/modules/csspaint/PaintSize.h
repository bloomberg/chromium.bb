// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintSize_h
#define PaintSize_h

#include "platform/bindings/ScriptWrappable.h"
#include "platform/geometry/IntSize.h"
#include "platform/heap/Handle.h"

namespace blink {

class PaintSize : public GarbageCollectedFinalized<PaintSize>,
                  public ScriptWrappable {
  WTF_MAKE_NONCOPYABLE(PaintSize);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static PaintSize* Create(IntSize size) { return new PaintSize(size); }
  virtual ~PaintSize() {}

  int width() const { return size_.Width(); }
  int height() const { return size_.Height(); }

  void Trace(blink::Visitor* visitor) {}

 private:
  explicit PaintSize(IntSize size) : size_(size) {}

  IntSize size_;
};

}  // namespace blink

#endif  // PaintSize_h
