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

#ifndef HTMLImportResourceOwner_h
#define HTMLImportResourceOwner_h

#include "core/fetch/RawResource.h"
#include "core/fetch/ResourcePtr.h"

namespace WebCore {

// A RawResourceClient implenetation which is responsible for
// owning RawResource object and adding/removing itself as a client
// according to its lifetime.
//
// FIXME: This could be geneeric ResouceOwner<ResourceType> once it is
// found that this class is broadly useful even outside HTMLImports module.
//
class HTMLImportResourceOwner : public RawResourceClient {
public:
    HTMLImportResourceOwner();
    virtual ~HTMLImportResourceOwner();

protected:
    const ResourcePtr<RawResource>& resource() const { return m_resource; }

    void setResource(const ResourcePtr<RawResource>&);
    void clearResource();
    bool hasResource() const { return m_resource.get(); }

private:
    ResourcePtr<RawResource> m_resource;
};

} // namespace WebCore

#endif // HTMLImportResourceOwner_h
