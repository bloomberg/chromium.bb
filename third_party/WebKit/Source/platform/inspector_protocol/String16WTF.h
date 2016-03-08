// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef String16WTF_h
#define String16WTF_h

#include "platform/Decimal.h"
#include "public/platform/WebString.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/StringConcatenate.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/UTF8.h"
#include "wtf/text/WTFString.h"

namespace blink {
namespace protocol {

class String16 {
public:
    String16() { }
    String16(const String16& other) : m_impl(other.m_impl) { }
    String16(const UChar* u, unsigned length) : m_impl(u, length) { }
    String16(const char* characters) : m_impl(characters) { }
    String16(const char* characters, size_t size) : m_impl(characters, size) { }
    String16(const WebString& other) : m_impl(other) { }
    template<typename StringType1, typename StringType2>
    String16(const WTF::StringAppend<StringType1, StringType2>& impl) : m_impl(impl) { }
    ~String16() { }

    // Integration constructors.
    String16(const WTF::String& impl) : m_impl(impl) { }
    String16(const WTF::AtomicString& impl) : m_impl(impl) { }
    String16(WTF::HashTableDeletedValueType) : m_impl(WTF::HashTableDeletedValue) { }

    static String16 fromUTF8(const char* characters, size_t length) { return String::fromUTF8(characters, length); }
    static String16 number(int i) { return String::number(i); }
    static String16 fromDouble(double number) { return Decimal::fromDouble(number).toString(); }
    static String16 createUninitialized(unsigned length, UChar*& data) { return String::createUninitialized(length, data); }
    static bool codePointCompareLessThan(const String16& a, const String16& b)
    {
        return codePointCompare(a.impl(), b.impl()) < 0;
    }

    typedef enum {
        conversionOK,
        sourceExhausted,
        targetExhausted,
        sourceIllegal
    } ConversionResult;

    static ConversionResult convertUTF8ToUTF16(const char** sourceStart, const char* sourceEnd, UChar** targetStart, UChar* targetEnd, bool* isSourceAllASCII = 0, bool strict = true)
    {
        return static_cast<ConversionResult>(WTF::Unicode::convertUTF8ToUTF16(sourceStart, sourceEnd, targetStart, targetEnd, isSourceAllASCII, strict));
    }

    bool validateUTF8()
    {
        return !m_impl.utf8(StrictUTF8Conversion).isNull();
    }

    size_t length() const { return m_impl.length(); }
    bool isEmpty() const { return m_impl.isEmpty(); }
    bool isNull() const { return m_impl.isNull(); }
    UChar operator[](unsigned index) const { return m_impl[index]; }

    bool is8Bit() const { return m_impl.is8Bit(); }
    unsigned sizeInBytes() const { return m_impl.sizeInBytes(); }
    const LChar* characters8() const { return m_impl.characters8(); }
    const UChar* characters16() const { return m_impl.characters16(); }
    String16 latin1Data()
    {
        CString latin1 = m_impl.latin1();
        // Include terminating zero.
        return String16(latin1.data(), latin1.length() + 1);
    }

    operator WTF::String() const { return m_impl; }
    operator WebString() const { return m_impl; }
    WTF::String impl() const { return m_impl; }

    static double charactersToDouble(const LChar* characters, size_t length, bool* ok = 0) { return ::charactersToDouble(characters, length, ok); }
    static double charactersToDouble(const UChar* characters, size_t length, bool* ok = 0) { return ::charactersToDouble(characters, length, ok); }

    String16 substring(unsigned pos, unsigned len = UINT_MAX) const { return m_impl.substring(pos, len); }
    String16 left(unsigned len) const { return m_impl.substring(0, len); }
    String16 stripWhiteSpace() const { return m_impl.stripWhiteSpace(); }

    int toInt(bool* ok = 0) const { return m_impl.toInt(ok); }
    unsigned toUInt(bool* ok = 0) const { return m_impl.toUInt(ok); }

    size_t find(UChar c, unsigned start = 0) const { return m_impl.find(c, start); }
    size_t find(const String16& str) const  { return m_impl.find(str.impl()); }
    size_t find(const String16& str, unsigned start) const { return m_impl.find(str.impl(), start); }
    size_t reverseFind(const String16& str, unsigned start = UINT_MAX) const { return m_impl.reverseFind(str.impl(), start); }

    void append(const String16& str) { m_impl.append(str); };
    bool isHashTableDeletedValue() const { return m_impl.isHashTableDeletedValue(); }

    bool startWith(const String16& s) const { return m_impl.startsWith(s); }
    bool startWith(UChar character) const { return m_impl.startsWith(character); }
    bool endsWith(const String16& s) const { return m_impl.endsWith(s); }
    bool endsWith(UChar character) const { return m_impl.endsWith(character); }

private:
    WTF::String m_impl;
};

class String16Builder {
public:
    String16Builder() { }
    void append(const String16& str) { m_impl.append(str); };
    void append(UChar c) { m_impl.append(c); };
    void append(LChar c) { m_impl.append(c); };
    void append(char c) { m_impl.append(c); };
    void append(const UChar* c, size_t size) { m_impl.append(c, size); };
    void append(const char* characters, unsigned length) { m_impl.append(characters, length); }
    void appendNumber(int number) { m_impl.appendNumber(number); }
    String16 toString() { return m_impl.toString(); }
    void reserveCapacity(unsigned newCapacity) { m_impl.reserveCapacity(newCapacity); }

private:
    WTF::StringBuilder m_impl;
};

inline bool operator==(const String16& a, const String16& b) { return a.impl() == b.impl(); }
inline bool operator!=(const String16& a, const String16& b) { return a.impl() != b.impl(); }
inline bool operator==(const String16& a, const char* b) { return a.impl() == b; }

inline String16 operator+(const String16& a, const char* b)
{
    return String(a.impl() + b);
}

inline String16 operator+(const char* a, const String16& b)
{
    return String(a + b.impl());
}

inline String16 operator+(const String16& a, const String16& b)
{
    return String(a.impl() + b.impl());
}

} // namespace protocol
} // namespace blink

using String16 = blink::protocol::String16;
using String16Builder = blink::protocol::String16Builder;

namespace WTF {

struct String16Hash {
    static unsigned hash(const String16& key) { return StringHash::hash(key.impl()); }
    static bool equal(const String16& a, const String16& b)
    {
        return StringHash::equal(a.impl(), b.impl());
    }
    static const bool safeToCompareToEmptyOrDeleted = false;
};

template<typename T> struct DefaultHash;
template<> struct DefaultHash<String16> {
    typedef String16Hash Hash;
};

template<>
struct HashTraits<String16> : SimpleClassHashTraits<String16> {
    static const bool hasIsEmptyValueFunction = true;
    static bool isEmptyValue(const String16& a) { return a.isNull(); }
};

} // namespace WTF

#endif // !defined(String16WTF_h)
