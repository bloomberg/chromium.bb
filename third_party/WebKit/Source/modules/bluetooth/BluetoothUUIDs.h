// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothUUIDs_h
#define BluetoothUUIDs_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Heap.h"

namespace blink {

// The Bluetooth standard defines numbers that identify services,
// characteristics, descriptors, and other entities.
// This section provides javascript names for UUIDs so they don't
// need to be replicated in each application.
class BluetoothUUIDs final
    : public GarbageCollected<BluetoothUUIDs>
    , public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:

    // Interface required by Garbage Collection:
    DEFINE_INLINE_TRACE() { }
};

} // namespace blink

#endif // BluetoothUUIDs_h
