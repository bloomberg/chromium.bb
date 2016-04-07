// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorCacheStorageAgent_h
#define InspectorCacheStorageAgent_h

#include "core/inspector/InspectorBaseAgent.h"
#include "modules/ModulesExport.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {


class MODULES_EXPORT InspectorCacheStorageAgent final : public InspectorBaseAgent<InspectorCacheStorageAgent, protocol::Frontend::CacheStorage>, public protocol::Backend::CacheStorage {
    WTF_MAKE_NONCOPYABLE(InspectorCacheStorageAgent);

public:
    static InspectorCacheStorageAgent* create()
    {
        return new InspectorCacheStorageAgent();
    }

    ~InspectorCacheStorageAgent() override;

    DECLARE_VIRTUAL_TRACE();

    void requestCacheNames(ErrorString*, const String& in_securityOrigin, PassOwnPtr<RequestCacheNamesCallback>) override;
    void requestEntries(ErrorString*, const String& in_cacheId, int in_skipCount, int in_pageSize, PassOwnPtr<RequestEntriesCallback>) override;
    void deleteCache(ErrorString*, const String& in_cacheId, PassOwnPtr<DeleteCacheCallback>) override;
    void deleteEntry(ErrorString*, const String& in_cacheId, const String& in_request, PassOwnPtr<DeleteEntryCallback>) override;

private:
    explicit InspectorCacheStorageAgent();
};

} // namespace blink

#endif // InspectorCacheStorageAgent_h
