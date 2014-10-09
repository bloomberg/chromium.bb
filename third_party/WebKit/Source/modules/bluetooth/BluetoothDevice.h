// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothDevice_h
#define BluetoothDevice_h

namespace blink {

class BluetoothDevice final
    : public GarbageCollected<BluetoothDevice>
    , public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    void trace(Visitor*) { }
};

} // namespace blink

#endif // BluetoothDevice_h
