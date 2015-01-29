// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMArrayPiece_h
#define DOMArrayPiece_h

#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMArrayBufferView.h"
#include "wtf/ArrayPiece.h"

namespace blink {

class DOMArrayPiece : public WTF::ArrayPiece {
public:
    DOMArrayPiece() { }
    DOMArrayPiece(DOMArrayBuffer* buffer)
        : ArrayPiece(buffer->buffer()) { }
    DOMArrayPiece(DOMArrayBufferView* view)
        : ArrayPiece(view->view()) { }
    template <class T>
    DOMArrayPiece(const T&);

    bool operator==(const DOMArrayBuffer& other) const
    {
        return byteLength() == other.byteLength() && memcmp(data(), other.data(), byteLength()) == 0;
    }

    bool operator==(const DOMArrayBufferView& other) const
    {
        return byteLength() == other.byteLength() && memcmp(data(), other.baseAddress(), byteLength()) == 0;
    }
};

} // namespace blink

#endif // DOMArrayPiece_h
