// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothSupplement_h
#define BluetoothSupplement_h

#include "core/frame/LocalFrame.h"

namespace blink {

class WebBluetooth;

// This class is attached to a LocalFrame in WebLocalFrameImpl::setCoreFrame, to
// pass WebFrameClient::bluetooth() (accessible by web/ only) to the Bluetooth
// code in modules/.
class BLINK_EXPORT BluetoothSupplement : public NoBaseWillBeGarbageCollected<BluetoothSupplement>, public WillBeHeapSupplement<LocalFrame> {
    WTF_MAKE_NONCOPYABLE(BluetoothSupplement);
    WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED(BluetoothSupplement);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(BluetoothSupplement);

public:
    static const char* supplementName();

    static void provideTo(LocalFrame&, WebBluetooth*);

    // Returns the WebBluetooth attached to the ScriptState's frame if that exists;
    // otherwise the Platform's WebBluetooth.
    static WebBluetooth* from(ScriptState*);

    DECLARE_VIRTUAL_TRACE();

private:
    explicit BluetoothSupplement(WebBluetooth*);

    WebBluetooth* m_bluetooth;
};

} // namespace blink

#endif // BluetoothRoutingId_h
