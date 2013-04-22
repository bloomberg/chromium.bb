/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2013 Apple Computer, Inc.  All rights reserved.
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

#ifndef ResourceHandleClient_h
#define ResourceHandleClient_h

#include <wtf/PassRefPtr.h>

namespace WebCore {
    class ResourceHandle;
    class ResourceError;
    class ResourceRequest;
    class ResourceResponse;

    enum CacheStoragePolicy {
        StorageAllowed,
        StorageAllowedInMemoryOnly,
        StorageNotAllowed
    };
    
    class ResourceHandleClient {
    public:
        ResourceHandleClient();
        virtual ~ResourceHandleClient();

        // Request may be modified.
        virtual void willSendRequest(ResourceHandle*, ResourceRequest&, const ResourceResponse& /*redirectResponse*/) { }
        virtual void didSendData(ResourceHandle*, unsigned long long /*bytesSent*/, unsigned long long /*totalBytesToBeSent*/) { }

        virtual void didReceiveResponse(ResourceHandle*, const ResourceResponse&) { }
        
        virtual void didReceiveData(ResourceHandle*, const char*, int, int /*encodedDataLength*/) { }
        
        virtual void didReceiveCachedMetadata(ResourceHandle*, const char*, int) { }
        virtual void didFinishLoading(ResourceHandle*, double /*finishTime*/) { }
        virtual void didFail(ResourceHandle*, const ResourceError&) { }

        virtual void didDownloadData(ResourceHandle*, int /*dataLength*/) { }
    };

}

#endif
