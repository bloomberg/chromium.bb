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

#include "core/fileapi/File.h"
#include "platform/blob/BlobRegistry.h"
#include "platform/blob/BlobURL.h"

namespace WebCore {

namespace {

class BlobURLRegistry : public URLRegistry {
public:
    virtual void registerURL(SecurityOrigin*, const KURL&, URLRegistrable*) OVERRIDE;
    virtual void unregisterURL(const KURL&) OVERRIDE;

    static URLRegistry& registry();
};

void BlobURLRegistry::registerURL(SecurityOrigin* origin, const KURL& publicURL, URLRegistrable* blob)
{
    ASSERT(&blob->registry() == this);
    BlobRegistry::registerPublicBlobURL(origin, publicURL, static_cast<Blob*>(blob)->blobDataHandle());
}

void BlobURLRegistry::unregisterURL(const KURL& publicURL)
{
    BlobRegistry::revokePublicBlobURL(publicURL);
}

URLRegistry& BlobURLRegistry::registry()
{
    DEFINE_STATIC_LOCAL(BlobURLRegistry, instance, ());
    return instance;
}

} // namespace

Blob::Blob(PassRefPtr<BlobDataHandle> dataHandle)
    : m_blobDataHandle(dataHandle)
{
    ScriptWrappable::init(this);
}

Blob::~Blob()
{
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
        size = this->size();
        ASSERT(size != -1);
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
            blobData->appendFileSystemURL(toFile(this)->fileSystemURL(), start, length, modificationTime);
        else
            blobData->appendFile(toFile(this)->path(), start, length, modificationTime);
    } else {
        blobData->appendBlob(m_blobDataHandle, start, length);
    }
    return Blob::create(BlobDataHandle::create(blobData.release(), length));
}

URLRegistry& Blob::registry() const
{
    return BlobURLRegistry::registry();
}


} // namespace WebCore
