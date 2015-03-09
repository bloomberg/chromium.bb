/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FetchContext_h
#define FetchContext_h

#include "core/fetch/CachePolicy.h"
#include "core/fetch/FetchInitiatorInfo.h"
#include "core/fetch/Resource.h"
#include "platform/heap/Handle.h"
#include "platform/network/ResourceLoadPriority.h"
#include "wtf/Noncopyable.h"

namespace blink {

class Document;
class DocumentLoader;
class LocalFrame;
class KURL;
class ResourceError;
class ResourceLoader;
class ResourceResponse;
class ResourceRequest;

enum FetchResourceType {
    FetchMainResource,
    FetchSubresource
};

class FetchContext : public NoBaseWillBeGarbageCollectedFinalized<FetchContext> {
    WTF_MAKE_NONCOPYABLE(FetchContext);
public:
    static FetchContext& nullInstance();

    FetchContext() { }
    virtual ~FetchContext() { }
    DEFINE_INLINE_VIRTUAL_TRACE() { }

    // Temporary while we remove references to these classes from ResourceFetcher.
    virtual LocalFrame* frame() const { return nullptr; }
    virtual Document* document() const { return nullptr; }
    virtual DocumentLoader* documentLoader() const { return nullptr; }

    virtual void reportLocalLoadFailed(const KURL&);
    virtual void addAdditionalRequestHeaders(ResourceRequest&, FetchResourceType);
    virtual void setFirstPartyForCookies(ResourceRequest&);
    virtual CachePolicy cachePolicy() const;
    virtual ResourceRequestCachePolicy resourceRequestCachePolicy(const ResourceRequest&, Resource::Type) const;

    virtual void dispatchDidChangeResourcePriority(unsigned long identifier, ResourceLoadPriority, int intraPriorityValue);
    virtual void dispatchWillSendRequest(unsigned long identifier, ResourceRequest&, const ResourceResponse& redirectResponse, const FetchInitiatorInfo& = FetchInitiatorInfo());
    virtual void dispatchDidLoadResourceFromMemoryCache(const ResourceRequest&, const ResourceResponse&);
    virtual void dispatchDidReceiveResponse(unsigned long identifier, const ResourceResponse&, ResourceLoader* = 0);
    virtual void dispatchDidReceiveData(unsigned long identifier, const char* data, int dataLength, int encodedDataLength);
    virtual void dispatchDidDownloadData(unsigned long identifier, int dataLength, int encodedDataLength);
    virtual void dispatchDidFinishLoading(unsigned long identifier, double finishTime, int64_t encodedDataLength);
    virtual void dispatchDidFail(unsigned long identifier, const ResourceError&, bool isInternalRequest);
    virtual void sendRemainingDelegateMessages(unsigned long identifier, const ResourceResponse&, int dataLength);
};

}

#endif
