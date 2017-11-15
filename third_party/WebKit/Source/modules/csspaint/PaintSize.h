// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintSize_h
#define PaintSize_h

#include "platform/bindings/ScriptWrappable.h"
#include "platform/geometry/FloatSize.h"
#include "platform/heap/Handle.h"

namespace blink {

class PaintSize : public ScriptWrappable {
  WTF_MAKE_NONCOPYABLE(PaintSize);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static PaintSize* Create(FloatSize size) { return new PaintSize(size); }
  virtual ~PaintSize() {}

  float width() const { return size_.Width(); }
  float height() const { return size_.Height(); }

 private:
  explicit PaintSize(FloatSize size) : size_(size) {}

  FloatSize size_;
};

}  // namespace blink

#endif  // PaintSize_h
