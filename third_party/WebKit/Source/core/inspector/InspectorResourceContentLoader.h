// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorResourceContentLoader_h
#define InspectorResourceContentLoader_h

#include "core/fetch/ResourcePtr.h"
#include "wtf/HashSet.h"
#include "wtf/Noncopyable.h"
#include "wtf/Vector.h"

namespace blink {

class LocalFrame;
class Resource;
class VoidCallback;

class InspectorResourceContentLoader final : public NoBaseWillBeGarbageCollectedFinalized<InspectorResourceContentLoader> {
    WTF_MAKE_NONCOPYABLE(InspectorResourceContentLoader);
    WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED;
public:
    explicit InspectorResourceContentLoader(LocalFrame*);
    void ensureResourcesContentLoaded(VoidCallback*);
    ~InspectorResourceContentLoader();
    DECLARE_TRACE();
    void dispose();
    bool hasFinished();
    void stop();

private:
    class ResourceClient;

    void resourceFinished(ResourceClient*);
    void checkDone();
    void start();

    PersistentHeapVectorWillBeHeapVector<Member<VoidCallback> > m_callbacks;
    bool m_allRequestsStarted;
    bool m_started;
    RawPtrWillBeMember<LocalFrame> m_inspectedFrame;
    HashSet<ResourceClient*> m_pendingResourceClients;
    Vector<ResourcePtr<Resource> > m_resources;

    friend class ResourceClient;
};

} // namespace blink


#endif // !defined(InspectorResourceContentLoader_h)
