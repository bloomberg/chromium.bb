// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorServiceWorkerCacheAgent_h
#define InspectorServiceWorkerCacheAgent_h

#include "core/InspectorFrontend.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

typedef String ErrorString;

class ServiceWorkerGlobalScope;

class InspectorServiceWorkerCacheAgent final : public InspectorBaseAgent<InspectorServiceWorkerCacheAgent, InspectorFrontend::ServiceWorkerCache>, public InspectorBackendDispatcher::ServiceWorkerCacheCommandHandler {
    WTF_MAKE_NONCOPYABLE(InspectorServiceWorkerCacheAgent);

public:
    static PassOwnPtrWillBeRawPtr<InspectorServiceWorkerCacheAgent> create(ServiceWorkerGlobalScope* workerGlobalScope)
    {
        return adoptPtrWillBeNoop(new InspectorServiceWorkerCacheAgent(workerGlobalScope));
    }

    virtual ~InspectorServiceWorkerCacheAgent();

    DECLARE_VIRTUAL_TRACE();

    virtual void requestCacheNames(ErrorString*, PassRefPtrWillBeRawPtr<RequestCacheNamesCallback>) override;
    virtual void requestEntries(ErrorString*, const String& cacheName, int skipCount, int pageSize, PassRefPtrWillBeRawPtr<RequestEntriesCallback>) override;
    virtual void deleteCache(ErrorString*, const String& cacheName, PassRefPtrWillBeRawPtr<DeleteCacheCallback>) override;

private:
    explicit InspectorServiceWorkerCacheAgent(ServiceWorkerGlobalScope*);

    ServiceWorkerGlobalScope* m_globalScope;
};

} // namespace blink

#endif // InspectorServiceWorkerCacheAgent_h
