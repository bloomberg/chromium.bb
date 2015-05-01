// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothGATTService_h
#define BluetoothGATTService_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

// Represents a GATT Service within a Bluetooth Peripheral, a collection of
// characteristics and relationships to other services that encapsulate the
// behavior of part of a device.
class BluetoothGATTService final
    : public GarbageCollected<BluetoothGATTService>
    , public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    DEFINE_INLINE_TRACE() { }
};

} // namespace blink

#endif // BluetoothGATTService_h
