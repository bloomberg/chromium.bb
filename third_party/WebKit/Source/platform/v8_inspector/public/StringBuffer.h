// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: this include guard is very specific to not clash with wtf. Will be changed after moving out of blink.
#ifndef v8_inspector_public_StringBuffer_h
#define v8_inspector_public_StringBuffer_h

#include "platform/PlatformExport.h"
#include "platform/v8_inspector/public/StringView.h"

#include <memory>

namespace v8_inspector {

class PLATFORM_EXPORT StringBuffer {
public:
    virtual ~StringBuffer() { }
    virtual const StringView& string() = 0;
    // This method copies contents.
    static std::unique_ptr<StringBuffer> create(const StringView&);
};

} // namespace v8_inspector

#endif // v8_inspector_public_StringBuffer_h
