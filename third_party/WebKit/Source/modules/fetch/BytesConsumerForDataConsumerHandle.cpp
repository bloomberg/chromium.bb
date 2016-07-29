// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/BytesConsumerForDataConsumerHandle.h"

#include <algorithm>
#include <string.h>

namespace blink {

BytesConsumerForDataConsumerHandle::BytesConsumerForDataConsumerHandle(std::unique_ptr<FetchDataConsumerHandle> handle)
    : m_reader(handle->obtainFetchDataReader(this))
{
}

BytesConsumerForDataConsumerHandle::~BytesConsumerForDataConsumerHandle() {}

BytesConsumer::Result BytesConsumerForDataConsumerHandle::read(char* buffer, size_t size, size_t* readSize)
{
    *readSize = 0;
    if (m_state == InternalState::Closed)
        return Result::Done;
    if (m_state == InternalState::Errored)
        return Result::Error;

    WebDataConsumerHandle::Result r = m_reader->read(buffer, size, WebDataConsumerHandle::FlagNone, readSize);
    switch (r) {
    case WebDataConsumerHandle::Ok:
        return Result::Ok;
    case WebDataConsumerHandle::ShouldWait:
        return Result::ShouldWait;
    case WebDataConsumerHandle::Done:
        close();
        return Result::Done;
    case WebDataConsumerHandle::Busy:
    case WebDataConsumerHandle::ResourceExhausted:
    case WebDataConsumerHandle::UnexpectedError:
        error();
        return Result::Error;
    }
    NOTREACHED();
    return Result::Error;
}

BytesConsumer::Result BytesConsumerForDataConsumerHandle::beginRead(const char** buffer, size_t* available)
{
    *buffer = nullptr;
    *available = 0;
    if (m_state == InternalState::Closed)
        return Result::Done;
    if (m_state == InternalState::Errored)
        return Result::Error;

    WebDataConsumerHandle::Result r = m_reader->beginRead(reinterpret_cast<const void**>(buffer), WebDataConsumerHandle::FlagNone, available);
    switch (r) {
    case WebDataConsumerHandle::Ok:
        return Result::Ok;
    case WebDataConsumerHandle::ShouldWait:
        return Result::ShouldWait;
    case WebDataConsumerHandle::Done:
        close();
        return Result::Done;
    case WebDataConsumerHandle::Busy:
    case WebDataConsumerHandle::ResourceExhausted:
    case WebDataConsumerHandle::UnexpectedError:
        error();
        return Result::Error;
    }
    NOTREACHED();
    return Result::Error;
}

BytesConsumer::Result BytesConsumerForDataConsumerHandle::endRead(size_t read)
{
    DCHECK(m_state == InternalState::Readable || m_state == InternalState::Waiting);
    WebDataConsumerHandle::Result r = m_reader->endRead(read);
    if (r == WebDataConsumerHandle::Ok)
        return Result::Ok;
    error();
    return Result::Error;
}

PassRefPtr<BlobDataHandle> BytesConsumerForDataConsumerHandle::drainAsBlobDataHandle(BlobSizePolicy policy)
{
    if (!m_reader)
        return nullptr;

    RefPtr<BlobDataHandle> handle;
    if (policy == BlobSizePolicy::DisallowBlobWithInvalidSize) {
        handle = m_reader->drainAsBlobDataHandle(FetchDataConsumerHandle::Reader::DisallowBlobWithInvalidSize);
    } else {
        DCHECK_EQ(BlobSizePolicy::AllowBlobWithInvalidSize, policy);
        handle = m_reader->drainAsBlobDataHandle(FetchDataConsumerHandle::Reader::AllowBlobWithInvalidSize);
    }

    if (handle)
        close();
    return handle.release();
}

PassRefPtr<EncodedFormData> BytesConsumerForDataConsumerHandle::drainAsFormData()
{
    if (!m_reader)
        return nullptr;
    RefPtr<EncodedFormData> formData = m_reader->drainAsFormData();
    if (formData)
        close();
    return formData.release();
}

void BytesConsumerForDataConsumerHandle::setClient(BytesConsumer::Client* client)
{
    DCHECK(!m_client);
    DCHECK(client);
    m_client = client;
}

void BytesConsumerForDataConsumerHandle::clearClient()
{
    DCHECK(m_client);
    m_client = nullptr;
}

void BytesConsumerForDataConsumerHandle::cancel()
{
    if (m_state == InternalState::Readable || m_state == InternalState::Waiting) {
        // We don't want the client to be notified in this case.
        BytesConsumer::Client* client = m_client;
        m_client = nullptr;
        close();
        m_client = client;
    }
}

BytesConsumer::PublicState BytesConsumerForDataConsumerHandle::getPublicState() const
{
    return getPublicStateFromInternalState(m_state);
}

void BytesConsumerForDataConsumerHandle::didGetReadable()
{
    DCHECK(m_state == InternalState::Readable || m_state == InternalState::Waiting);
    // Perform zero-length read to call check handle's status.
    size_t readSize;
    WebDataConsumerHandle::Result result = m_reader->read(nullptr, 0, WebDataConsumerHandle::FlagNone, &readSize);
    switch (result) {
    case WebDataConsumerHandle::Ok:
    case WebDataConsumerHandle::ShouldWait:
        if (m_client)
            m_client->onStateChange();
        return;
    case WebDataConsumerHandle::Done:
        close();
        if (m_client)
            m_client->onStateChange();
        return;
    case WebDataConsumerHandle::Busy:
    case WebDataConsumerHandle::ResourceExhausted:
    case WebDataConsumerHandle::UnexpectedError:
        error();
        if (m_client)
            m_client->onStateChange();
        return;
    }
    return;
}

DEFINE_TRACE(BytesConsumerForDataConsumerHandle)
{
    visitor->trace(m_client);
    BytesConsumer::trace(visitor);
}

void BytesConsumerForDataConsumerHandle::close()
{
    if (m_state == InternalState::Closed)
        return;
    DCHECK(m_state == InternalState::Readable || m_state == InternalState::Waiting);
    m_state = InternalState::Closed;
    m_reader = nullptr;
}

void BytesConsumerForDataConsumerHandle::error()
{
    if (m_state == InternalState::Errored)
        return;
    DCHECK(m_state == InternalState::Readable || m_state == InternalState::Waiting);
    m_state = InternalState::Errored;
    m_reader = nullptr;
    m_error = Error("error");
}

} // namespace blink
