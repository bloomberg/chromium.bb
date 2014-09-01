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

class Page;
class Resource;
class VoidCallback;

// FIXME: Oilpan: Move InspectorResourceContentLoader to the heap.
class InspectorResourceContentLoader FINAL {
    WTF_MAKE_NONCOPYABLE(InspectorResourceContentLoader);
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit InspectorResourceContentLoader(Page*);
    void ensureResourcesContentLoaded(PassOwnPtrWillBeRawPtr<VoidCallback>);
    ~InspectorResourceContentLoader();
    bool hasFinished();
    void stop();

private:
    class ResourceClient;

    void resourceFinished(ResourceClient*);
    void checkDone();
    void start();

    WillBePersistentHeapVector<OwnPtrWillBeMember<VoidCallback> > m_callbacks;
    bool m_allRequestsStarted;
    bool m_started;
    Page* m_page;
    HashSet<ResourceClient*> m_pendingResourceClients;
    Vector<ResourcePtr<Resource> > m_resources;

    friend class ResourceClient;
};

} // namespace blink


#endif // !defined(InspectorResourceContentLoader_h)
