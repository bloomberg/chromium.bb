// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/bluetooth/ConvertWebVectorToArrayBuffer.h"

#include "wtf/OwnPtr.h"

namespace blink {

PassRefPtr<DOMArrayBuffer> ConvertWebVectorToArrayBuffer::take(ScriptPromiseResolver*, WebVector<uint8_t>* webVectorRawPointer)
{
    OwnPtr<WebVector<uint8_t>> webVector = adoptPtr(webVectorRawPointer);

    static_assert(sizeof(*webVector->data()) == 1, "uint8_t should be a single byte");

    RefPtr<DOMArrayBuffer> domBuffer = DOMArrayBuffer::create(webVector->data(), webVector->size());

    return domBuffer;
}

void ConvertWebVectorToArrayBuffer::dispose(WebVector<uint8_t>* webBufferRaw)
{
    delete webBufferRaw;
}

} // namespace blink
