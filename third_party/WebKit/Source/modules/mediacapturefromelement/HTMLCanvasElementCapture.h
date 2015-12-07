// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HTMLCanvasElementCapture_h
#define HTMLCanvasElementCapture_h

#include "core/html/HTMLCanvasElement.h"
#include "platform/heap/Handle.h"
#include "wtf/Allocator.h"

namespace blink {

class CanvasCaptureMediaStream;
class ExceptionState;

class HTMLCanvasElementCapture {
    STATIC_ONLY(HTMLCanvasElementCapture);
public:
    static CanvasCaptureMediaStream* captureStream(HTMLCanvasElement&, ExceptionState&);
};

} // namespace blink

#endif
