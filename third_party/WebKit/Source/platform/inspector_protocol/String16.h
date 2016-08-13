// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef String16_h
#define String16_h

#include "platform/inspector_protocol/Collections.h"
#include "platform/inspector_protocol/Platform.h"

#include <vector>

namespace blink {
namespace protocol {

namespace internal {
PLATFORM_EXPORT void intToStr(int, char*, size_t);
PLATFORM_EXPORT void doubleToStr(double, char*, size_t);
PLATFORM_EXPORT void doubleToStr3(double, char*, size_t);
PLATFORM_EXPORT void doubleToStr6(double, char*, size_t);
PLATFORM_EXPORT double strToDouble(const char*, bool*);
PLATFORM_EXPORT int strToInt(const char*, bool*);
} // namespace internal

template <typename T, typename C>
class PLATFORM_EXPORT String16Base {
public:
    static bool isASCII(C c)
    {
        return !(c & ~0x7F);
    }

    static bool isSpaceOrNewLine(C c)
    {
        return isASCII(c) && c <= ' ' && (c == ' ' || (c <= 0xD && c >= 0x9));
    }

    static T fromInteger(int number)
    {
        char buffer[50];
        internal::intToStr(number, buffer, PROTOCOL_ARRAY_LENGTH(buffer));
        return T(buffer);
    }

    static T fromDouble(double number)
    {
        char buffer[100];
        internal::doubleToStr(number, buffer, PROTOCOL_ARRAY_LENGTH(buffer));
        return T(buffer);
    }

    static T fromDoublePrecision3(double number)
    {
        char buffer[100];
        internal::doubleToStr3(number, buffer, PROTOCOL_ARRAY_LENGTH(buffer));
        return T(buffer);
    }

    static T fromDoublePrecision6(double number)
    {
        char buffer[100];
        internal::doubleToStr6(number, buffer, PROTOCOL_ARRAY_LENGTH(buffer));
        return T(buffer);
    }

    static double charactersToDouble(const C* characters, size_t length, bool* ok = nullptr)
    {
        std::vector<char> buffer;
        buffer.reserve(length + 1);
        for (size_t i = 0; i < length; ++i) {
            if (!isASCII(characters[i])) {
                if (ok)
                    *ok = false;
                return 0;
            }
            buffer.push_back(static_cast<char>(characters[i]));
        }
        buffer.push_back('\0');
        return internal::strToDouble(buffer.data(), ok);
    }

    static int charactersToInteger(const C* characters, size_t length, bool* ok = nullptr)
    {
        std::vector<char> buffer;
        buffer.reserve(length + 1);
        for (size_t i = 0; i < length; ++i) {
            if (!isASCII(characters[i])) {
                if (ok)
                    *ok = false;
                return 0;
            }
            buffer.push_back(static_cast<char>(characters[i]));
        }
        buffer.push_back('\0');
        return internal::strToInt(buffer.data(), ok);
    }

    double toDouble(bool* ok = nullptr) const
    {
        const C* characters = static_cast<const T&>(*this).characters16();
        size_t length = static_cast<const T&>(*this).length();
        return charactersToDouble(characters, length, ok);
    }

    int toInteger(bool* ok = nullptr) const
    {
        const C* characters = static_cast<const T&>(*this).characters16();
        size_t length = static_cast<const T&>(*this).length();
        return charactersToInteger(characters, length, ok);
    }

    T stripWhiteSpace() const
    {
        size_t length = static_cast<const T&>(*this).length();
        if (!length)
            return T();

        unsigned start = 0;
        unsigned end = length - 1;
        const C* characters = static_cast<const T&>(*this).characters16();

        // skip white space from start
        while (start <= end && isSpaceOrNewLine(characters[start]))
            ++start;

        // only white space
        if (start > end)
            return T();

        // skip white space from end
        while (end && isSpaceOrNewLine(characters[end]))
            --end;

        if (!start && end == length - 1)
            return T(static_cast<const T&>(*this));
        return T(characters + start, end + 1 - start);
    }

    bool startsWith(const char* prefix) const
    {
        const C* characters = static_cast<const T&>(*this).characters16();
        size_t length = static_cast<const T&>(*this).length();
        for (size_t i = 0, j = 0; prefix[j] && i < length; ++i, ++j) {
            if (characters[i] != prefix[j])
                return false;
        }
        return true;
    }
};

} // namespace protocol
} // namespace blink

#if V8_INSPECTOR_USE_STL
#include "platform/inspector_protocol/String16STL.h"
#else
#include "platform/inspector_protocol/String16WTF.h"
#endif // V8_INSPECTOR_USE_STL

namespace blink {
namespace protocol {

class PLATFORM_EXPORT String16Builder {
public:
    String16Builder();
    void append(const String16&);
    void append(UChar);
    void append(char);
    void append(const UChar*, size_t);
    void append(const char*, size_t);
    String16 toString();
    void reserveCapacity(size_t);

private:
    std::vector<UChar> m_buffer;
};

} // namespace protocol
} // namespace blink

using String16 = blink::protocol::String16;
using String16Builder = blink::protocol::String16Builder;

#endif // !defined(String16_h)
