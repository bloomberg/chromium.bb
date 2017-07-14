// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/typed_arrays/DOMArrayPiece.h"

#include "bindings/core/v8/ArrayBufferOrArrayBufferView.h"

namespace blink {

DOMArrayPiece::DOMArrayPiece(
    const ArrayBufferOrArrayBufferView& array_buffer_or_view,
    InitWithUnionOption option) {
  if (array_buffer_or_view.isArrayBuffer()) {
    DOMArrayBuffer* array_buffer = array_buffer_or_view.getAsArrayBuffer();
    InitWithData(array_buffer->Data(), array_buffer->ByteLength());
  } else if (array_buffer_or_view.isArrayBufferView()) {
    DOMArrayBufferView* array_buffer_view =
        array_buffer_or_view.getAsArrayBufferView().View();
    InitWithData(array_buffer_view->BaseAddress(),
                 array_buffer_view->byteLength());
  } else if (array_buffer_or_view.isNull() &&
             option == kAllowNullPointToNullWithZeroSize) {
    InitWithData(nullptr, 0);
  }  // Otherwise, leave the obejct as null.
}

}  // namespace blink
