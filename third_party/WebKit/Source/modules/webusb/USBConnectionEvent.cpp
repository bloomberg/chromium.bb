// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webusb/USBConnectionEvent.h"

#include "modules/webusb/USBConnectionEventInit.h"
#include "modules/webusb/USBDevice.h"

namespace blink {

PassRefPtrWillBeRawPtr<USBConnectionEvent> USBConnectionEvent::create(const AtomicString& type, const USBConnectionEventInit& initializer)
{
    return adoptRefWillBeNoop(new USBConnectionEvent(type, initializer));
}

PassRefPtrWillBeRawPtr<USBConnectionEvent> USBConnectionEvent::create(const AtomicString& type, USBDevice* device)
{
    return adoptRefWillBeNoop(new USBConnectionEvent(type, device));
}

USBConnectionEvent::USBConnectionEvent(const AtomicString& type, const USBConnectionEventInit& initializer)
    : Event(type, initializer)
    , m_device(nullptr)
{
    if (initializer.hasDevice())
        m_device = initializer.device();
}

USBConnectionEvent::USBConnectionEvent(const AtomicString& type, USBDevice* device)
    : Event(type, false, false)
    , m_device(device)
{
}

DEFINE_TRACE(USBConnectionEvent)
{
    visitor->trace(m_device);
    Event::trace(visitor);
}

} // namespace blink
