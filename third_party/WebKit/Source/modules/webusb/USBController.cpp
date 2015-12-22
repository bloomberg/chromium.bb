// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webusb/USBController.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "public/platform/modules/webusb/WebUSBClient.h"

namespace blink {

USBController::~USBController()
{
}

void USBController::provideTo(LocalFrame& frame, WebUSBClient* client)
{
    ASSERT(RuntimeEnabledFeatures::webUSBEnabled());
    USBController* controller = new USBController(frame, client);
    WillBeHeapSupplement<LocalFrame>::provideTo(frame, supplementName(), adoptPtrWillBeNoop(controller));
}

USBController& USBController::from(LocalFrame& frame)
{
    USBController* controller = static_cast<USBController*>(WillBeHeapSupplement<LocalFrame>::from(frame, supplementName()));
    ASSERT(controller);
    return *controller;
}

const char* USBController::supplementName()
{
    return "USBController";
}

USBController::USBController(LocalFrame& frame, WebUSBClient* client)
    : LocalFrameLifecycleObserver(&frame)
    , m_client(client)
{
}

void USBController::willDetachFrameHost()
{
    m_client = nullptr;
}

DEFINE_TRACE(USBController)
{
    WillBeHeapSupplement<LocalFrame>::trace(visitor);
    LocalFrameLifecycleObserver::trace(visitor);
}

} // namespace blink
