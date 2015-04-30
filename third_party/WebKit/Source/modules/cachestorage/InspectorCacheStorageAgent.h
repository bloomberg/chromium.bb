// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorCacheStorageAgent_h
#define InspectorCacheStorageAgent_h

#include "core/InspectorFrontend.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

typedef String ErrorString;

class InspectorCacheStorageAgent final : public InspectorBaseAgent<InspectorCacheStorageAgent, InspectorFrontend::CacheStorage>, public InspectorBackendDispatcher::CacheStorageCommandHandler {
    WTF_MAKE_NONCOPYABLE(InspectorCacheStorageAgent);

public:
    static PassOwnPtrWillBeRawPtr<InspectorCacheStorageAgent> create()
    {
        return adoptPtrWillBeNoop(new InspectorCacheStorageAgent());
    }

    virtual ~InspectorCacheStorageAgent();

    DECLARE_VIRTUAL_TRACE();

    virtual void requestCacheNames(ErrorString*, const String& securityOrigin, PassRefPtrWillBeRawPtr<RequestCacheNamesCallback>) override;
    virtual void requestEntries(ErrorString*, const String& cacheId, int skipCount, int pageSize, PassRefPtrWillBeRawPtr<RequestEntriesCallback>) override;
    virtual void deleteCache(ErrorString*, const String& cacheId, PassRefPtrWillBeRawPtr<DeleteCacheCallback>) override;
    virtual void deleteEntry(ErrorString*, const String& cacheId, const String& request, PassRefPtrWillBeRawPtr<DeleteEntryCallback>) override;

private:
    explicit InspectorCacheStorageAgent();
};

} // namespace blink

#endif // InspectorCacheStorageAgent_h
