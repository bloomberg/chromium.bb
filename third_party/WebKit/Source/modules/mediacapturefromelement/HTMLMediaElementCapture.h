// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HTMLMediaElementCapture_h
#define HTMLMediaElementCapture_h

#include "platform/wtf/Allocator.h"

namespace blink {

class ExceptionState;
class HTMLMediaElement;
class MediaStream;
class ScriptState;

class HTMLMediaElementCapture {
  STATIC_ONLY(HTMLMediaElementCapture);

 public:
  static MediaStream* captureStream(ScriptState*,
                                    HTMLMediaElement&,
                                    ExceptionState&);
};

}  // namespace blink

#endif
