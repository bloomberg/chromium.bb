// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/BytesConsumer.h"

#include <algorithm>
#include <string.h>

namespace blink {

BytesConsumer::Result BytesConsumer::read(char* buffer, size_t size, size_t* readSize)
{
    *readSize = 0;
    const char* src = nullptr;
    size_t available;
    Result r = beginRead(&src, &available);
    if (r != Result::Ok)
        return r;
    *readSize = std::min(available, size);
    memcpy(buffer, src, *readSize);
    return endRead(*readSize);
}

} // namespace blink
