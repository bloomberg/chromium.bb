// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BeaconLoader_h
#define BeaconLoader_h

#include "core/loader/PingLoader.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"

namespace blink {

class Blob;
class DOMArrayBufferView;
class DOMFormData;
class KURL;
class LocalFrame;

// Issue asynchronous beacon transmission loads independent of LocalFrame
// staying alive. PingLoader providing the service.
class BeaconLoader final : public PingLoader {
    WTF_MAKE_NONCOPYABLE(BeaconLoader);
    WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED;
public:
    virtual ~BeaconLoader() { }

    static bool sendBeacon(LocalFrame*, int, const KURL&, const String&, int&);
    static bool sendBeacon(LocalFrame*, int, const KURL&, PassRefPtr<DOMArrayBufferView>, int&);
    static bool sendBeacon(LocalFrame*, int, const KURL&, Blob*, int&);
    static bool sendBeacon(LocalFrame*, int, const KURL&, PassRefPtrWillBeRawPtr<DOMFormData>, int&);

private:
    class Sender;
};

} // namespace blink

#endif // BeaconLoader_h
