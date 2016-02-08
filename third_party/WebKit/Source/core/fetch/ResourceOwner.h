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

#ifndef ResourceOwner_h
#define ResourceOwner_h

#include "core/fetch/Resource.h"

namespace blink {

template<class R, class C = typename R::ClientType>
class ResourceOwner : public WillBeGarbageCollectedMixin, public C {
    WILL_BE_USING_PRE_FINALIZER(ResourceOwner, clearResource);
public:
    using ResourceType = R;

    virtual ~ResourceOwner();
    ResourceType* resource() const { return m_resource.get(); }

    DEFINE_INLINE_VIRTUAL_TRACE() { visitor->trace(m_resource); }

protected:
    ResourceOwner();

    void setResource(const PassRefPtrWillBeRawPtr<ResourceType>&);
    void clearResource() { setResource(nullptr); }

private:
    RefPtrWillBeMember<ResourceType> m_resource;
};

template<class R, class C>
inline ResourceOwner<R, C>::ResourceOwner()
{
#if ENABLE(OILPAN)
    ThreadState::current()->registerPreFinalizer(this);
#endif
}

template<class R, class C>
inline ResourceOwner<R, C>::~ResourceOwner()
{
#if !ENABLE(OILPAN)
    clearResource();
#endif
}

template<class R, class C>
inline void ResourceOwner<R, C>::setResource(const PassRefPtrWillBeRawPtr<R>& newResource)
{
    if (newResource == m_resource)
        return;

    // Some ResourceClient implementations reenter this so
    // we need to prevent double removal.
    if (RefPtrWillBeRawPtr<ResourceType> oldResource = m_resource.release())
        oldResource->removeClient(this);

    if (newResource) {
        m_resource = newResource;
        m_resource->addClient(this);
    }
}

} // namespace blink

#endif
