// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HTMLCanvasElementCapture_h
#define HTMLCanvasElementCapture_h

#include "core/html/canvas/HTMLCanvasElement.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class MediaStream;
class ExceptionState;

class HTMLCanvasElementCapture {
  STATIC_ONLY(HTMLCanvasElementCapture);

 public:
  static MediaStream* captureStream(ScriptState*,
                                    HTMLCanvasElement&,
                                    ExceptionState&);
  static MediaStream* captureStream(ScriptState*,
                                    HTMLCanvasElement&,
                                    double frame_rate,
                                    ExceptionState&);

 private:
  static MediaStream* captureStream(ScriptState*,
                                    HTMLCanvasElement&,
                                    bool given_frame_rate,
                                    double frame_rate,
                                    ExceptionState&);
};

}  // namespace blink

#endif
