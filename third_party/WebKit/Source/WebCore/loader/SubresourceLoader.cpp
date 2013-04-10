/*
 * Copyright (C) 2006, 2007, 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SubresourceLoader.h"

#include "CachedResourceLoader.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "Logging.h"
#include "MemoryCache.h"
#include "ResourceBuffer.h"
#include "WebCoreMemoryInstrumentation.h"
#include <wtf/RefCountedLeakCounter.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>

namespace WebCore {

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, subresourceLoaderCounter, ("SubresourceLoader"));

SubresourceLoader::SubresourceLoader(Frame* frame, CachedResource* resource, const ResourceLoaderOptions& options)
    : ResourceLoader(frame, resource, options)
{
#ifndef NDEBUG
    subresourceLoaderCounter.increment();
#endif
}

SubresourceLoader::~SubresourceLoader()
{
    ASSERT(m_state != Initialized);
    ASSERT(reachedTerminalState());
#ifndef NDEBUG
    subresourceLoaderCounter.decrement();
#endif
}

PassRefPtr<SubresourceLoader> SubresourceLoader::create(Frame* frame, CachedResource* resource, const ResourceRequest& request, const ResourceLoaderOptions& options)
{
    RefPtr<SubresourceLoader> subloader(adoptRef(new SubresourceLoader(frame, resource, options)));
    if (!subloader->init(request))
        return 0;
    return subloader.release();
}

bool SubresourceLoader::init(const ResourceRequest& request)
{
    if (!ResourceLoader::init(request))
        return false;

    ASSERT(!reachedTerminalState());
    m_state = Initialized;
    m_documentLoader->addSubresourceLoader(this);
    return true;
}

void SubresourceLoader::didFinishLoading(double finishTime)
{
    if (m_state != Initialized)
        return;
    ASSERT(!reachedTerminalState());
    ASSERT(!m_resource->resourceToRevalidate());
    ASSERT(!m_resource->errorOccurred());
    LOG(ResourceLoading, "Received '%s'.", m_resource->url().string().latin1().data());

    RefPtr<SubresourceLoader> protect(this);
    CachedResourceHandle<CachedResource> protectResource(m_resource);
    m_state = Finishing;
    m_resource->setLoadFinishTime(finishTime);
    m_resource->data(resourceData(), true);
    m_resource->finish();
    ResourceLoader::didFinishLoading(finishTime);
}

void SubresourceLoader::didFail(const ResourceError& error)
{
    if (m_state != Initialized)
        return;
    ASSERT(!reachedTerminalState());
    LOG(ResourceLoading, "Failed to load '%s'.\n", m_resource->url().string().latin1().data());

    RefPtr<SubresourceLoader> protect(this);
    CachedResourceHandle<CachedResource> protectResource(m_resource);
    m_state = Finishing;
    if (m_resource->resourceToRevalidate())
        memoryCache()->revalidationFailed(m_resource);
    m_resource->setResourceError(error);
    m_resource->error(CachedResource::LoadError);
    if (!m_resource->isPreloaded())
        memoryCache()->remove(m_resource);
    ResourceLoader::didFail(error);
}

void SubresourceLoader::releaseResources()
{
    ASSERT(!reachedTerminalState());
    if (m_state != Uninitialized) {
        m_requestCountTracker.clear();
        m_documentLoader->cachedResourceLoader()->loadDone(m_resource);
        if (reachedTerminalState())
            return;
        m_resource->stopLoading();
        m_documentLoader->removeSubresourceLoader(this);
    }
    ResourceLoader::releaseResources();
}

}
