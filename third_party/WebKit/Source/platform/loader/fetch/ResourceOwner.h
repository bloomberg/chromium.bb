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

#include "platform/heap/Handle.h"
#include "platform/loader/fetch/Resource.h"

namespace blink {

template <class R, class C>
class GC_PLUGIN_IGNORE("https://crbug.com/652966") ResourceOwner;

template <class R, class C = typename R::ClientType>
class ResourceOwner : public C {
  USING_PRE_FINALIZER(ResourceOwner, ClearResource);

 public:
  using ResourceType = R;
  ~ResourceOwner() override {}
  ResourceType* GetResource() const { return resource_; }

  DEFINE_INLINE_TRACE() {
    visitor->Trace(resource_);
    C::Trace(visitor);
  }

 protected:
  ResourceOwner() {}

  void SetResource(ResourceType*);
  void ClearResource() { SetResource(nullptr); }

 private:
  Member<ResourceType> resource_;
};

template <class R, class C>
inline void ResourceOwner<R, C>::SetResource(R* new_resource) {
  if (new_resource == resource_)
    return;

  // Some ResourceClient implementations reenter this so
  // we need to prevent double removal.
  if (ResourceType* old_resource = resource_.Release())
    old_resource->RemoveClient(this);

  if (new_resource) {
    resource_ = new_resource;
    resource_->AddClient(this);
  }
}

}  // namespace blink

#endif
