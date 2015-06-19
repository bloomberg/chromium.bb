// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/fetch/FetchDataLoader.h"

#include "wtf/ArrayBufferBuilder.h"
#include "wtf/text/WTFString.h"

namespace blink {

namespace {

class FetchDataLoaderAsBlobHandle
    : public FetchDataLoader
    , public WebDataConsumerHandle::Client {
public:
    explicit FetchDataLoaderAsBlobHandle(const String& mimeType)
        : m_client(nullptr)
        , m_mimeType(mimeType) { }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        FetchDataLoader::trace(visitor);
        visitor->trace(m_client);
    }

private:
    void start(FetchDataConsumerHandle* handle, FetchDataLoader::Client* client) override
    {
        ASSERT(!m_client);
        ASSERT(!m_reader);

        m_client = client;
        // Passing |this| here is safe because |this| owns |m_reader|.
        m_reader = handle->obtainReader(this);
        RefPtr<BlobDataHandle> blobHandle = m_reader->drainAsBlobDataHandle();
        if (blobHandle) {
            ASSERT(blobHandle->size() != kuint64max);
            m_reader.clear();
            if (blobHandle->type() != m_mimeType) {
                // A new BlobDataHandle is created to override the Blob's type.
                m_client->didFetchDataLoadedBlobHandle(BlobDataHandle::create(blobHandle->uuid(), m_mimeType, blobHandle->size()));
            } else {
                m_client->didFetchDataLoadedBlobHandle(blobHandle);
            }
            m_client.clear();
            return;
        }

        // We read data from |m_reader| and create a new blob.
        m_blobData = BlobData::create();
        m_blobData->setContentType(m_mimeType);
    }

    void didGetReadable() override
    {
        ASSERT(m_client);
        ASSERT(m_reader);

        while (true) {
            const void* buffer;
            size_t available;
            WebDataConsumerHandle::Result result = m_reader->beginRead(&buffer, WebDataConsumerHandle::FlagNone, &available);

            switch (result) {
            case WebDataConsumerHandle::Ok:
                m_blobData->appendBytes(buffer, available);
                m_reader->endRead(available);
                break;

            case WebDataConsumerHandle::Done: {
                m_reader.clear();
                long long size = m_blobData->length();
                m_client->didFetchDataLoadedBlobHandle(BlobDataHandle::create(m_blobData.release(), size));
                m_client.clear();
                return;
            }

            case WebDataConsumerHandle::ShouldWait:
                return;

            case WebDataConsumerHandle::Busy:
            case WebDataConsumerHandle::ResourceExhausted:
            case WebDataConsumerHandle::UnexpectedError: {
                m_reader.clear();
                m_blobData.clear();
                m_client->didFetchDataLoadFailed();
                m_client.clear();
                return;
            }
            }
        }
    }

    void cancel() override
    {
        m_reader.clear();
        m_blobData.clear();
        m_client.clear();
    }

    OwnPtr<FetchDataConsumerHandle::Reader> m_reader;
    Member<FetchDataLoader::Client> m_client;

    String m_mimeType;
    OwnPtr<BlobData> m_blobData;
};

class FetchDataLoaderAsArrayBufferOrString
    : public FetchDataLoader
    , public WebDataConsumerHandle::Client {
public:
    enum LoadType {
        LoadAsArrayBuffer,
        LoadAsString
    };

    explicit FetchDataLoaderAsArrayBufferOrString(LoadType loadType)
        : m_client(nullptr)
        , m_loadType(loadType) { }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        FetchDataLoader::trace(visitor);
        visitor->trace(m_client);
    }

protected:
    void start(FetchDataConsumerHandle* handle, FetchDataLoader::Client* client) override
    {
        ASSERT(!m_client);
        ASSERT(!m_rawData);
        ASSERT(!m_reader);
        m_client = client;
        m_rawData = adoptPtr(new ArrayBufferBuilder());
        m_reader = handle->obtainReader(this);
    }

    void didGetReadable() override
    {
        ASSERT(m_client);
        ASSERT(m_rawData);
        ASSERT(m_reader);

        while (true) {
            const void* buffer;
            size_t available;
            WebDataConsumerHandle::Result result = m_reader->beginRead(&buffer, WebDataConsumerHandle::FlagNone, &available);

            switch (result) {
            case WebDataConsumerHandle::Ok:
                if (available > 0) {
                    unsigned bytesAppended = m_rawData->append(static_cast<const char*>(buffer), available);
                    if (!bytesAppended) {
                        m_reader->endRead(0);
                        error();
                        return;
                    }
                    ASSERT(bytesAppended == available);
                }
                m_reader->endRead(available);
                break;

            case WebDataConsumerHandle::Done: {
                m_reader.clear();
                switch (m_loadType) {
                case LoadAsArrayBuffer:
                    m_client->didFetchDataLoadedArrayBuffer(DOMArrayBuffer::create(m_rawData->toArrayBuffer()));
                    break;
                case LoadAsString:
                    m_client->didFetchDataLoadedString(m_rawData->toString());
                    break;
                }
                m_rawData.clear();
                m_client.clear();
                return;
            }

            case WebDataConsumerHandle::ShouldWait:
                return;

            case WebDataConsumerHandle::Busy:
            case WebDataConsumerHandle::ResourceExhausted:
            case WebDataConsumerHandle::UnexpectedError:
                error();
                return;
            }
        }
    }

    void error()
    {
        m_reader.clear();
        m_rawData.clear();
        m_client->didFetchDataLoadFailed();
        m_client.clear();
    }

    void cancel() override
    {
        m_reader.clear();
        m_rawData.clear();
        m_client.clear();
    }

    OwnPtr<FetchDataConsumerHandle::Reader> m_reader;
    Member<FetchDataLoader::Client> m_client;

    LoadType m_loadType;
    OwnPtr<ArrayBufferBuilder> m_rawData;
};

} // namespace

FetchDataLoader* FetchDataLoader::createLoaderAsBlobHandle(const String& mimeType)
{
    return new FetchDataLoaderAsBlobHandle(mimeType);
}

FetchDataLoader* FetchDataLoader::createLoaderAsArrayBuffer()
{
    return new FetchDataLoaderAsArrayBufferOrString(FetchDataLoaderAsArrayBufferOrString::LoadAsArrayBuffer);
}

FetchDataLoader* FetchDataLoader::createLoaderAsString()
{
    return new FetchDataLoaderAsArrayBufferOrString(FetchDataLoaderAsArrayBufferOrString::LoadAsString);
}

} // namespace blink
