// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothGATTCharacteristic_h
#define BluetoothGATTCharacteristic_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

// BluetoothGATTCharacteristic represents a GATT Characteristic, which is a
// basic data element that provides further information about a peripheral's
// service.
class BluetoothGATTCharacteristic final
    : public GarbageCollected<BluetoothGATTCharacteristic>
    , public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    // Interface required by garbage collection.
    DEFINE_INLINE_TRACE() { }
};

} // namespace blink

#endif // BluetoothGATTCharacteristic_h
