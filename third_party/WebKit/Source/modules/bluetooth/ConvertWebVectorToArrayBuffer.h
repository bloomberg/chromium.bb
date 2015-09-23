// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ConvertWebVectorToArrayBuffer_h
#define ConvertWebVectorToArrayBuffer_h

#include "core/dom/DOMArrayBuffer.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebVector.h"

namespace blink {

class ScriptPromiseResolver;

// ConvertWebVectorToArrayBuffer is used with CallbackPromiseAdapter to receive
// WebVector responses. See CallbackPromiseAdapter class comments.
class ConvertWebVectorToArrayBuffer {
    STATIC_ONLY(ConvertWebVectorToArrayBuffer);
public:
    // Interface required by CallbackPromiseAdapter:
    using WebType = const WebVector<uint8_t>&;
    static PassRefPtr<DOMArrayBuffer> take(ScriptPromiseResolver*, const WebVector<uint8_t>&);
};

} // namespace blink

#endif // ConvertWebVectorToArrayBuffer_h
