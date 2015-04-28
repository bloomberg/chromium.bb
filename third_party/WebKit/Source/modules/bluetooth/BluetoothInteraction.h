// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothInteraction_h
#define BluetoothInteraction_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/bluetooth/BluetoothUUIDs.h"
#include "platform/heap/Handle.h"

namespace blink {

// Object and UUID lookup on navigator.bluetooth
class BluetoothInteraction
    : public GarbageCollected<BluetoothInteraction>
    , public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:

    BluetoothUUIDs* uuids();

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_bluetoothUUIDs);
    }

private:
    Member<BluetoothUUIDs> m_bluetoothUUIDs;
};

} // namespace blink

#endif // BluetoothInteraction_h
