// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothUUIDsService_h
#define BluetoothUUIDsService_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

// Standarized services listed in
// https://developer.bluetooth.org/gatt/services/Pages/ServicesHome.aspx
class BluetoothUUIDsService final
    : public GarbageCollected<BluetoothUUIDsService>
    , public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:

    DEFINE_INLINE_TRACE() { }
};

} // namespace blink

#endif // BluetoothUUIDsServices_h
