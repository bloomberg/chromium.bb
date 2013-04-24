/*
 * Copyright (C) 2004, 2006, 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef ResourceHandle_h
#define ResourceHandle_h

#include "core/platform/network/ResourceHandleTypes.h"
#include "core/platform/network/ResourceLoadPriority.h"
#include <wtf/Forward.h>
#include <wtf/OwnPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {
class KURL;
class NetworkingContext;
class ResourceError;
class ResourceHandleClient;
class ResourceHandleInternal;
class ResourceRequest;
class ResourceResponse;

template <typename T> class Timer;

class ResourceHandle : public RefCounted<ResourceHandle> {
public:
    static PassRefPtr<ResourceHandle> create(NetworkingContext*, const ResourceRequest&, ResourceHandleClient*, bool defersLoading, bool shouldContentSniff, StoredCredentials);
    static void loadResourceSynchronously(NetworkingContext*, const ResourceRequest&, StoredCredentials, ResourceError&, ResourceResponse&, Vector<char>& data);

    static void cacheMetadata(const ResourceResponse&, const Vector<char>&);

    virtual ~ResourceHandle();

    virtual void cancel();

    // The client may be 0, in which case no callbacks will be made.
    ResourceHandleClient* client() const;
    void setClient(ResourceHandleClient*);

    void setDefersLoading(bool);

    void didChangePriority(ResourceLoadPriority);

    ResourceRequest& firstRequest();

    NetworkingContext* context() const;

    typedef PassRefPtr<ResourceHandle> (*BuiltinConstructor)(const ResourceRequest& request, ResourceHandleClient* client);
    static void registerBuiltinConstructor(const AtomicString& protocol, BuiltinConstructor);

    typedef void (*BuiltinSynchronousLoader)(NetworkingContext*, const ResourceRequest&, StoredCredentials, ResourceError&, ResourceResponse&, Vector<char>& data);
    static void registerBuiltinSynchronousLoader(const AtomicString& protocol, BuiltinSynchronousLoader);

protected:
    ResourceHandle(NetworkingContext*, const ResourceRequest&, ResourceHandleClient*, bool defersLoading, bool shouldContentSniff);

private:
    bool start(StoredCredentials);
    friend class ResourceHandleInternal;
    OwnPtr<ResourceHandleInternal> d;
};

}

#endif // ResourceHandle_h
