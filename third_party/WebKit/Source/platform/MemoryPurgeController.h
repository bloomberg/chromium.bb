// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MemoryPurgeController_h
#define MemoryPurgeController_h

#include "platform/PlatformExport.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "wtf/MainThread.h"

namespace blink {

enum class MemoryPurgeMode {
    // The tab contains the webview went to background
    InactiveTab,
    // TODO(bashi): Add more modes as needed.
};

enum class DeviceKind {
    NotSpecified,
    LowEnd,
};

// Classes which have discardable/reducible memory can implement this
// interface to be informed when they should reduce memory consumption.
// MemoryPurgeController assumes that subclasses of MemoryPurgeClient are
// WillBes.
class PLATFORM_EXPORT MemoryPurgeClient : public WillBeGarbageCollectedMixin {
public:
    virtual ~MemoryPurgeClient() { }

    // MemoryPurgeController invokes this callback when a memory purge event
    // has occurred.
    virtual void purgeMemory(MemoryPurgeMode, DeviceKind) = 0;

    DEFINE_INLINE_VIRTUAL_TRACE() { }
};

// MemoryPurgeController listens to some events which could be opportunities
// for reducing memory consumption and notifies its clients.
// Since we want to control memory per tab, MemoryPurgeController is owned by
// Page.
class PLATFORM_EXPORT MemoryPurgeController final : public NoBaseWillBeGarbageCollected<MemoryPurgeController> {
    DECLARE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(MemoryPurgeController);
public:
    static PassOwnPtrWillBeRawPtr<MemoryPurgeController> create()
    {
        return adoptPtrWillBeNoop(new MemoryPurgeController);
    }

    void registerClient(MemoryPurgeClient* client)
    {
        ASSERT(isMainThread());
        ASSERT(client);
        ASSERT(!m_clients.contains(client));
        m_clients.add(client);
    }

    void unregisterClient(MemoryPurgeClient* client)
    {
        ASSERT(isMainThread());
        ASSERT(m_clients.contains(client));
        m_clients.remove(client);
    }

    void pageBecameActive();
    void pageBecameInactive();
    void pageInactiveTask(Timer<MemoryPurgeController>*);

    DECLARE_TRACE();

private:
    MemoryPurgeController();

    void purgeMemory(MemoryPurgeMode);

    WillBeHeapHashSet<RawPtrWillBeWeakMember<MemoryPurgeClient>> m_clients;
    DeviceKind m_deviceKind;
    Timer<MemoryPurgeController> m_inactiveTimer;
};

} // namespace blink

#endif // MemoryPurgeController_h
