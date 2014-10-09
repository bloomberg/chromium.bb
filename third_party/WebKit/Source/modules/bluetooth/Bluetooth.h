// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Bluetooth_h
#define Bluetooth_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/bluetooth/BluetoothDiscovery.h"

namespace blink {

class Bluetooth final
    : public BluetoothDiscovery {
    DEFINE_WRAPPERTYPEINFO();
public:
    static Bluetooth* create()
    {
        return new Bluetooth();
    }
};

} // namespace blink

#endif // Bluetooth_h
