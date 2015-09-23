// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScreenWakeLock_h
#define ScreenWakeLock_h

#include "core/frame/LocalFrameLifecycleObserver.h"
#include "core/page/PageLifecycleObserver.h"
#include "modules/ModulesExport.h"
#include "wtf/Noncopyable.h"

namespace blink {

class LocalFrame;
class Screen;
class WebWakeLockClient;

class MODULES_EXPORT ScreenWakeLock final : public NoBaseWillBeGarbageCollected<ScreenWakeLock>, public WillBeHeapSupplement<LocalFrame>, public PageLifecycleObserver, public LocalFrameLifecycleObserver {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(ScreenWakeLock);
    WTF_MAKE_NONCOPYABLE(ScreenWakeLock);
public:
    static bool keepAwake(Screen&);
    static void setKeepAwake(Screen&, bool);

    bool keepAwake() const { return m_keepAwake; }
    void setKeepAwake(bool);

    static const char* supplementName();
    static ScreenWakeLock* from(LocalFrame*);
    static void provideTo(LocalFrame&, WebWakeLockClient*);

    // Inherited from PageLifecycleObserver.
    void pageVisibilityChanged() override;
    void didCommitLoad(LocalFrame*) override;

    // Inherited from LocalFrameLifecycleObserver.
    void willDetachFrameHost() override;

    DECLARE_VIRTUAL_TRACE();

private:
    ScreenWakeLock(LocalFrame&, WebWakeLockClient*);

    static ScreenWakeLock* fromScreen(Screen&);
    void notifyClient();

    WebWakeLockClient* m_client;
    bool m_keepAwake;
};

} // namespace blink

#endif // ScreenWakeLock_h
