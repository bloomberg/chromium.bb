// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorResourceContentLoader_h
#define InspectorResourceContentLoader_h

#include "core/CoreExport.h"
#include "core/fetch/ResourcePtr.h"
#include "wtf/Functional.h"
#include "wtf/HashSet.h"
#include "wtf/Noncopyable.h"
#include "wtf/Vector.h"

namespace blink {

class LocalFrame;
class Resource;

class CORE_EXPORT InspectorResourceContentLoader final : public NoBaseWillBeGarbageCollectedFinalized<InspectorResourceContentLoader> {
    WTF_MAKE_NONCOPYABLE(InspectorResourceContentLoader);
    USING_FAST_MALLOC_WILL_BE_REMOVED(InspectorResourceContentLoader);
public:
    static PassOwnPtrWillBeRawPtr<InspectorResourceContentLoader> create(LocalFrame* inspectedFrame)
    {
        return adoptPtrWillBeNoop(new InspectorResourceContentLoader(inspectedFrame));
    }
    ~InspectorResourceContentLoader();
    void dispose();
    DECLARE_TRACE();

    void ensureResourcesContentLoaded(PassOwnPtr<Closure> callback);
    void didCommitLoadForLocalFrame(LocalFrame*);

private:
    class ResourceClient;

    explicit InspectorResourceContentLoader(LocalFrame*);
    void resourceFinished(ResourceClient*);
    void checkDone();
    void start();
    void stop();
    bool hasFinished();

    Vector<OwnPtr<Closure>> m_callbacks;
    bool m_allRequestsStarted;
    bool m_started;
    RawPtrWillBeMember<LocalFrame> m_inspectedFrame;
    WillBeHeapHashSet<RawPtrWillBeMember<ResourceClient>> m_pendingResourceClients;
    Vector<ResourcePtr<Resource>> m_resources;

    friend class ResourceClient;
};

} // namespace blink


#endif // !defined(InspectorResourceContentLoader_h)
