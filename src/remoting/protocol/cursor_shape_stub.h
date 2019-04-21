// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Interface for an object that receives cursor shape events.

#ifndef REMOTING_PROTOCOL_CURSOR_SHAPE_STUB_H_
#define REMOTING_PROTOCOL_CURSOR_SHAPE_STUB_H_

#include "base/macros.h"

namespace remoting {
namespace protocol {

class CursorShapeInfo;

class CursorShapeStub {
 public:
  CursorShapeStub() {}
  virtual ~CursorShapeStub() {}

  virtual void SetCursorShape(const CursorShapeInfo& cursor_shape) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CursorShapeStub);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CURSOR_SHAPE_STUB_H_
