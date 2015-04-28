// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothInteraction_h
#define BluetoothInteraction_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

// Object and UUID lookup on navigator.bluetooth
class BluetoothInteraction
    : public GarbageCollected<BluetoothInteraction>
    , public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:

    DEFINE_INLINE_TRACE() { }
};

} // namespace blink

#endif // BluetoothInteraction
