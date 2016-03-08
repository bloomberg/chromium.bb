// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/installedapp/InstalledAppController.h"

#include "core/frame/LocalFrame.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "public/platform/WebSecurityOrigin.h"

namespace blink {

InstalledAppController::~InstalledAppController()
{
}

void InstalledAppController::provideTo(LocalFrame& frame, WebInstalledAppClient* client)
{
    ASSERT(RuntimeEnabledFeatures::installedAppEnabled());

    OwnPtrWillBeRawPtr<InstalledAppController> controller = adoptPtrWillBeNoop(new InstalledAppController(frame, client));
    WillBeHeapSupplement<LocalFrame>::provideTo(frame, supplementName(), controller.release());
}

InstalledAppController* InstalledAppController::from(LocalFrame& frame)
{
    InstalledAppController* controller = static_cast<InstalledAppController*>(WillBeHeapSupplement<LocalFrame>::from(frame, supplementName()));
    ASSERT(controller);
    return controller;
}

InstalledAppController::InstalledAppController(LocalFrame& frame, WebInstalledAppClient* client)
    : LocalFrameLifecycleObserver(&frame)
    , m_client(client)
{
}

const char* InstalledAppController::supplementName()
{
    return "InstalledAppController";
}

void InstalledAppController::getInstalledApps(const WebSecurityOrigin& url, WebPassOwnPtr<AppInstalledCallbacks> callback)
{
    // When detached, the client is no longer valid.
    if (!m_client) {
        callback.release()->onError();
        return;
    }

    // Client is expected to take ownership of the callback
    m_client->getInstalledRelatedApps(url, callback);
}

void InstalledAppController::willDetachFrameHost()
{
    m_client = nullptr;
}

DEFINE_TRACE(InstalledAppController)
{
    WillBeHeapSupplement<LocalFrame>::trace(visitor);
    LocalFrameLifecycleObserver::trace(visitor);
}

} // namespace blink
