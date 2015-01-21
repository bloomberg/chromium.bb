// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/dom/DOMArrayBufferDeallocationObserver.h"

#include "wtf/Threading.h"
#include <v8.h>

namespace blink {

DOMArrayBufferDeallocationObserver& DOMArrayBufferDeallocationObserver::instance()
{
    AtomicallyInitializedStaticReference(DOMArrayBufferDeallocationObserver, deallocationObserver, new DOMArrayBufferDeallocationObserver);
    return deallocationObserver;
}

void DOMArrayBufferDeallocationObserver::arrayBufferDeallocated(unsigned sizeInBytes)
{
    v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(-static_cast<int>(sizeInBytes));
}

void DOMArrayBufferDeallocationObserver::blinkAllocatedMemory(unsigned sizeInBytes)
{
    v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(static_cast<int>(sizeInBytes));
}

} // namespace blink
