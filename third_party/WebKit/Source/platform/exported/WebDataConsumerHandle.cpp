// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebDataConsumerHandle.h"

#include "platform/heap/Handle.h"

#include <algorithm>
#include <string.h>

namespace blink {

WebDataConsumerHandle::WebDataConsumerHandle()
{
    ASSERT(ThreadState::current());
}

WebDataConsumerHandle::~WebDataConsumerHandle()
{
    ASSERT(ThreadState::current());
}

PassOwnPtr<WebDataConsumerHandle::Reader> WebDataConsumerHandle::obtainReader(WebDataConsumerHandle::Client* client)
{
    ASSERT(ThreadState::current());
    return adoptPtr(obtainReaderInternal(client));
}

WebDataConsumerHandle::Result WebDataConsumerHandle::Reader::read(void* data, size_t size, Flags flags, size_t* readSize)
{
    *readSize = 0;
    const void* src = nullptr;
    size_t available;
    Result r = beginRead(&src, flags, &available);
    if (r != WebDataConsumerHandle::Ok)
        return r;
    *readSize = std::min(available, size);
    memcpy(data, src, *readSize);
    return endRead(*readSize);
}

} // namespace blink

