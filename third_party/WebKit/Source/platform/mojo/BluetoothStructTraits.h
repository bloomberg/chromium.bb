// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothStructTraits_h
#define BluetoothStructTraits_h

#include "device/bluetooth/public/interfaces/uuid.mojom-blink.h"
#include "wtf/text/WTFString.h"

namespace mojo {

template <>
struct StructTraits<bluetooth::mojom::UUIDDataView, WTF::String> {
  static const WTF::String& uuid(const WTF::String& input) { return input; }

  static bool Read(bluetooth::mojom::UUIDDataView, WTF::String* output);
};

}  // namespace mojo

#endif  // BluetoothStructTraits_h
