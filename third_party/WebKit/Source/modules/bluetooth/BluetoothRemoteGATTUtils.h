// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothRemoteGATTUtils_h
#define BluetoothRemoteGATTUtils_h

#include "core/dom/DOMDataView.h"
#include "wtf/Vector.h"

namespace blink {

class BluetoothRemoteGATTUtils final {
 public:
  static DOMDataView* ConvertWTFVectorToDataView(const WTF::Vector<uint8_t>&);
};

}  // namespace blink

#endif  // BluetoothRemoteGATTUtils_h
