// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USBController_h
#define USBController_h

#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameLifecycleObserver.h"
#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class WebUSBClient;

class MODULES_EXPORT USBController final
    : public NoBaseWillBeGarbageCollectedFinalized<USBController>
    , public WillBeHeapSupplement<LocalFrame>
    , public LocalFrameLifecycleObserver {
    WTF_MAKE_NONCOPYABLE(USBController);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(USBController);
public:
    virtual ~USBController();

    WebUSBClient* client() { return m_client; }

    static void provideTo(LocalFrame&, WebUSBClient*);
    static USBController& from(LocalFrame&);
    static const char* supplementName();

    DECLARE_VIRTUAL_TRACE();

private:
    USBController(LocalFrame&, WebUSBClient*);

    // Inherited from LocalFrameLifecycleObserver.
    void willDetachFrameHost() override;

    WebUSBClient* m_client;
};

} // namespace blink

#endif // USBController_h
