// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothGATTRemoteServer_h
#define BluetoothGATTRemoteServer_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Heap.h"

namespace blink {

// BluetoothGATTRemoteServer provides a way to interact with a connected bluetooth peripheral.
class BluetoothGATTRemoteServer final
    : public GarbageCollected<BluetoothGATTRemoteServer>
    , public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:

    // Interface required by Garbage Collectoin:
    DEFINE_INLINE_TRACE() { }
};

} // namespace blink

#endif // BluetoothDevice_h
