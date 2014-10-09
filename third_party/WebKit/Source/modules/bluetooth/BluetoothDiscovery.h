// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothDiscovery_h
#define BluetoothDiscovery_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Visitor.h"

namespace blink {

class ScriptPromise;
class ScriptState;

class BluetoothDiscovery
    : public GarbageCollected<BluetoothDiscovery>
    , public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    ScriptPromise requestDevice(ScriptState*);

    void trace(Visitor*) { }
};

} // namespace blink

#endif // BluetoothDiscovery_h
