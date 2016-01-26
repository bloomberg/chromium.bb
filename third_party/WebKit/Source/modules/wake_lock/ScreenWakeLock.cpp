// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/wake_lock/ScreenWakeLock.h"

#include "core/frame/LocalFrame.h"
#include "core/frame/Screen.h"
#include "core/page/PageVisibilityState.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "public/platform/modules/wake_lock/WebWakeLockClient.h"

namespace blink {

// static
bool ScreenWakeLock::keepAwake(Screen& screen)
{
    ScreenWakeLock* screenWakeLock = fromScreen(screen);
    if (!screenWakeLock)
        return false;

    return screenWakeLock->keepAwake();
}

// static
void ScreenWakeLock::setKeepAwake(Screen& screen, bool keepAwake)
{
    ScreenWakeLock* screenWakeLock = fromScreen(screen);
    if (!screenWakeLock)
        return;

    screenWakeLock->setKeepAwake(keepAwake);
}

void ScreenWakeLock::setKeepAwake(bool keepAwake)
{
    m_keepAwake = keepAwake;
    notifyClient();
}

// static
const char* ScreenWakeLock::supplementName()
{
    return "ScreenWakeLock";
}

// static
ScreenWakeLock* ScreenWakeLock::from(LocalFrame* frame)
{
    return static_cast<ScreenWakeLock*>(WillBeHeapSupplement<LocalFrame>::from(frame, supplementName()));
}

// static
void ScreenWakeLock::provideTo(LocalFrame& frame, WebWakeLockClient* client)
{
    ASSERT(RuntimeEnabledFeatures::wakeLockEnabled());
    WillBeHeapSupplement<LocalFrame>::provideTo(
        frame,
        ScreenWakeLock::supplementName(),
        adoptPtrWillBeNoop(new ScreenWakeLock(frame, client)));
}

void ScreenWakeLock::pageVisibilityChanged()
{
    notifyClient();
}

void ScreenWakeLock::didCommitLoad(LocalFrame* committedFrame)
{
    // Reset wake lock flag for this frame if it is the one being navigated.
    if (committedFrame == frame()) {
        setKeepAwake(false);
    }
}

void ScreenWakeLock::willDetachFrameHost()
{
    m_client = nullptr;
}

DEFINE_TRACE(ScreenWakeLock)
{
    WillBeHeapSupplement<LocalFrame>::trace(visitor);
    PageLifecycleObserver::trace(visitor);
    LocalFrameLifecycleObserver::trace(visitor);
}

ScreenWakeLock::ScreenWakeLock(LocalFrame& frame, WebWakeLockClient* client)
    : PageLifecycleObserver(frame.page())
    , LocalFrameLifecycleObserver(&frame)
    , m_client(client)
    , m_keepAwake(false)
{
}

// static
ScreenWakeLock* ScreenWakeLock::fromScreen(Screen& screen)
{
    return screen.frame() ? ScreenWakeLock::from(screen.frame()) : nullptr;
}

void ScreenWakeLock::notifyClient()
{
    if (!m_client)
        return;

    m_client->requestKeepScreenAwake(m_keepAwake && page() && page()->isPageVisible());
}

} // namespace blink
