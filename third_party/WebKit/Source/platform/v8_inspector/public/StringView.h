// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: this include guard is very specific to not clash with wtf. Will be changed after moving out of blink.
#ifndef v8_inspector_public_StringView_h
#define v8_inspector_public_StringView_h

#include "platform/PlatformExport.h"

#include <cctype>
#include <stdint.h>

namespace v8_inspector {

class PLATFORM_EXPORT StringView {
public:
    StringView()
        : m_is8Bit(true)
        , m_length(0)
        , m_characters8(nullptr) {}

    StringView(const uint8_t* characters, unsigned length)
        : m_is8Bit(true)
        , m_length(length)
        , m_characters8(characters) {}

    StringView(const uint16_t* characters, unsigned length)
        : m_is8Bit(false)
        , m_length(length)
        , m_characters16(characters) {}

    bool is8Bit() const { return m_is8Bit; }
    unsigned length() const { return m_length; }

    // TODO(dgozman): add DCHECK(m_is8Bit) to accessors once platform can be used here.
    const uint8_t* characters8() const { return m_characters8; }
    const uint16_t* characters16() const { return m_characters16; }

private:
    bool m_is8Bit;
    unsigned m_length;
    union {
        const uint8_t* m_characters8;
        const uint16_t* m_characters16;
    };
};

} // namespace v8_inspector

#endif // v8_inspector_public_StringView_h
