/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "config.h"
#include "core/fileapi/Blob.h"

#include "core/fileapi/BlobRegistry.h"
#include "core/fileapi/BlobURL.h"
#include "core/fileapi/File.h"

namespace WebCore {

class BlobURLRegistry : public URLRegistry {
public:
    virtual void registerURL(SecurityOrigin*, const KURL&, URLRegistrable*) OVERRIDE;
    virtual void unregisterURL(const KURL&) OVERRIDE;

    static URLRegistry& registry();
};


void BlobURLRegistry::registerURL(SecurityOrigin* origin, const KURL& publicURL, URLRegistrable* blob)
{
    ASSERT(&blob->registry() == this);
    BlobRegistry::registerBlobURL(origin, publicURL, static_cast<Blob*>(blob)->url());
}

void BlobURLRegistry::unregisterURL(const KURL& url)
{
    BlobRegistry::unregisterBlobURL(url);
}

URLRegistry& BlobURLRegistry::registry()
{
    DEFINE_STATIC_LOCAL(BlobURLRegistry, instance, ());
    return instance;
}


Blob::Blob()
    : m_size(0)
{
    ScriptWrappable::init(this);
    OwnPtr<BlobData> blobData = BlobData::create();

    // Create a new internal URL and register it with the provided blob data.
    m_internalURL = BlobURL::createInternalURL();
    BlobRegistry::registerBlobURL(m_internalURL, blobData.release());
}

Blob::Blob(PassOwnPtr<BlobData> blobData, long long size)
    : m_type(blobData->contentType())
    , m_size(size)
{
    ASSERT(blobData);
    ScriptWrappable::init(this);

    // Create a new internal URL and register it with the provided blob data.
    m_internalURL = BlobURL::createInternalURL();
    BlobRegistry::registerBlobURL(m_internalURL, blobData);
}

Blob::Blob(const KURL& srcURL, const String& type, long long size)
    : m_type(type)
    , m_size(size)
{
    ScriptWrappable::init(this);

    // Create a new internal URL and register it with the same blob data as the source URL.
    m_internalURL = BlobURL::createInternalURL();
    BlobRegistry::registerBlobURL(0, m_internalURL, srcURL);
}

Blob::~Blob()
{
    BlobRegistry::unregisterBlobURL(m_internalURL);
}

PassRefPtr<Blob> Blob::slice(long long start, long long end, const String& contentType) const
{
    // When we slice a file for the first time, we obtain a snapshot of the file by capturing its current size and modification time.
    // The modification time will be used to verify if the file has been changed or not, when the underlying data are accessed.
    long long size;
    double modificationTime;
    if (isFile()) {
        // FIXME: This involves synchronous file operation. We need to figure out how to make it asynchronous.
        toFile(this)->captureSnapshot(size, modificationTime);
    } else {
        ASSERT(m_size != -1);
        size = m_size;
    }

    // Convert the negative value that is used to select from the end.
    if (start < 0)
        start = start + size;
    if (end < 0)
        end = end + size;

    // Clamp the range if it exceeds the size limit.
    if (start < 0)
        start = 0;
    if (end < 0)
        end = 0;
    if (start >= size) {
        start = 0;
        end = 0;
    } else if (end < start)
        end = start;
    else if (end > size)
        end = size;

    long long length = end - start;
    OwnPtr<BlobData> blobData = BlobData::create();
    blobData->setContentType(contentType);
    if (isFile()) {
        if (!toFile(this)->fileSystemURL().isEmpty())
            blobData->appendURL(toFile(this)->fileSystemURL(), start, length, modificationTime);
        else
            blobData->appendFile(toFile(this)->path(), start, length, modificationTime);
    } else
        blobData->appendBlob(m_internalURL, start, length);

    return Blob::create(blobData.release(), length);
}

URLRegistry& Blob::registry() const
{
    return BlobURLRegistry::registry();
}


} // namespace WebCore
