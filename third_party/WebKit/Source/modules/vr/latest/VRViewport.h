// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VRViewport_h
#define VRViewport_h

#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"

namespace blink {

class VRViewport final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  VRViewport(int x, int y, int width, int height)
      : x_(x), y_(y), width_(width), height_(height) {}

  int x() const { return x_; }
  int y() const { return y_; }
  int width() const { return width_; }
  int height() const { return height_; }

 private:
  int x_;
  int y_;
  int width_;
  int height_;
};

}  // namespace blink

#endif  // VRViewport_h
