// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMArrayBufferDeallocationObserver_h
#define DOMArrayBufferDeallocationObserver_h

#include "wtf/ArrayBufferDeallocationObserver.h"

namespace blink {

class DOMArrayBufferDeallocationObserver final : public WTF::ArrayBufferDeallocationObserver {
public:
    static DOMArrayBufferDeallocationObserver& instance();

    virtual void arrayBufferDeallocated(unsigned sizeInBytes) override;

protected:
    virtual void blinkAllocatedMemory(unsigned sizeInBytes) override;
};

} // namespace blink

#endif // DOMArrayBufferDeallocationObserver_h
