// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/bluetooth/BluetoothRemoteGATTUtils.h"

namespace blink {

// static
DOMDataView* BluetoothRemoteGATTUtils::ConvertWTFVectorToDataView(
    const WTF::Vector<uint8_t>& wtfVector) {
  static_assert(sizeof(*wtfVector.data()) == 1,
                "uint8_t should be a single byte");
  DOMArrayBuffer* domBuffer =
      DOMArrayBuffer::create(wtfVector.data(), wtfVector.size());
  return DOMDataView::create(domBuffer, 0, wtfVector.size());
}

}  // namespace blink
