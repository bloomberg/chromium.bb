// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WTF_StringView_h
#define WTF_StringView_h

#include "wtf/Allocator.h"
#if DCHECK_IS_ON()
#include "wtf/RefPtr.h"
#endif
#include "wtf/text/AtomicString.h"
#include "wtf/text/StringImpl.h"
#include "wtf/text/Unicode.h"
#include "wtf/text/WTFString.h"
#include <cstring>

namespace WTF {

// A string like object that wraps either an 8bit or 16bit byte sequence
// and keeps track of the length and the type, it does NOT own the bytes.
//
// Since StringView does not own the bytes creating a StringView from a String,
// then calling clear() on the string will result in a use-after-free. Asserts
// in ~StringView attempt to enforce this for most common cases.
//
// See base/strings/string_piece.h for more details.
class WTF_EXPORT StringView {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
public:
    // Null string.
    StringView() { clear(); }

    // From a StringView:
    StringView(const StringView&, unsigned offset, unsigned length);
    StringView(const StringView& view, unsigned offset)
        : StringView(view, offset, view.m_length - offset) {}

    // From a StringImpl:
    StringView(StringImpl*);
    StringView(StringImpl*, unsigned offset);
    StringView(StringImpl*, unsigned offset, unsigned length);

    // From a String:
    StringView(const String& string, unsigned offset, unsigned length)
        : StringView(string.impl(), offset, length) {}
    StringView(const String& string, unsigned offset)
        : StringView(string.impl(), offset) {}
    StringView(const String& string)
        : StringView(string, 0, string.length()) {}

    // From an AtomicString:
    StringView(const AtomicString& string, unsigned offset, unsigned length)
        : StringView(string.impl(), offset, length) {}
    StringView(const AtomicString& string, unsigned offset)
        : StringView(string.impl(), offset) {}
    StringView(const AtomicString& string)
        : StringView(string, 0, string.length()) {}

    // From a literal string or LChar buffer:
    StringView(const LChar* chars, unsigned length);
    StringView(const char* chars, unsigned length)
        : StringView(reinterpret_cast<const LChar*>(chars), length) {}
    StringView(const LChar* chars)
        : StringView(chars, chars ? strlen(reinterpret_cast<const char*>(chars)) : 0) {}
    StringView(const char* chars)
        : StringView(reinterpret_cast<const LChar*>(chars)) {}

    // From a wide literal string or UChar buffer.
    StringView(const UChar* chars);
    StringView(const UChar* chars, unsigned length);

    // From a byte pointer.
    StringView(const void* bytes, unsigned length, bool is8Bit)
        : m_length(length)
        , m_is8Bit(is8Bit)
    {
        m_data.bytes = bytes;
    }

#if DCHECK_IS_ON()
    ~StringView();
#endif

    bool isNull() const { return !m_data.bytes; }
    bool isEmpty() const { return !m_length; }

    unsigned length() const { return m_length; }

    bool is8Bit() const { return m_is8Bit; }

    void clear();

    UChar operator[](unsigned i) const
    {
        SECURITY_DCHECK(i < length());
        if (is8Bit())
            return characters8()[i];
        return characters16()[i];
    }

    const LChar* characters8() const
    {
        ASSERT(is8Bit());
        return m_data.characters8;
    }

    const UChar* characters16() const
    {
        ASSERT(!is8Bit());
        return m_data.characters16;
    }

    const void* bytes() const { return m_data.bytes; }

    String toString() const;
    AtomicString toAtomicString() const;

private:
    void set(StringImpl&, unsigned offset, unsigned length);

#if DCHECK_IS_ON()
    RefPtr<StringImpl> m_impl;
#endif
    union DataUnion {
        const LChar* characters8;
        const UChar* characters16;
        const void* bytes;
    } m_data;
    unsigned m_length;
    unsigned m_is8Bit : 1;
};

inline StringView::StringView(const StringView& view, unsigned offset, unsigned length)
    : m_length(length)
    , m_is8Bit(view.is8Bit())
{
    SECURITY_DCHECK(offset + length <= view.length());
    if (is8Bit())
        m_data.characters8 = view.characters8() + offset;
    else
        m_data.characters16 = view.characters16() + offset;
}

inline StringView::StringView(StringImpl* impl)
{
    impl ? set(*impl, 0, impl->length()) : clear();
}

inline StringView::StringView(StringImpl* impl, unsigned offset)
{
    impl ? set(*impl, offset, impl->length() - offset) : clear();
}

inline StringView::StringView(StringImpl* impl, unsigned offset, unsigned length)
{
    impl ? set(*impl, offset, length) : clear();
}

inline StringView::StringView(const LChar* chars, unsigned length)
    : m_length(length)
    , m_is8Bit(true)
{
    m_data.characters8 = chars;
}

inline void StringView::clear()
{
    m_length = 0;
    m_is8Bit = true;
    m_data.bytes = nullptr;
#if DCHECK_IS_ON()
    m_impl = nullptr;
#endif
}

inline void StringView::set(StringImpl& impl, unsigned offset, unsigned length)
{
    SECURITY_DCHECK(offset + length <= impl.length());
    m_length = length;
    m_is8Bit = impl.is8Bit();
#if DCHECK_IS_ON()
    m_impl = &impl;
#endif
    if (m_is8Bit)
        m_data.characters8 = impl.characters8() + offset;
    else
        m_data.characters16 = impl.characters16() + offset;
}

WTF_EXPORT bool equalIgnoringASCIICase(const StringView& a, const StringView& b);

// TODO(esprehn): Can't make this an overload of WTF::equal since that makes
// calls to equal() that pass literal strings ambiguous. Figure out if we can
// replace all the callers with equalStringView and then rename it to equal().
WTF_EXPORT bool equalStringView(const StringView&, const StringView&);

inline bool operator==(const StringView& a, const StringView& b)
{
    return equalStringView(a, b);
}

inline bool operator!=(const StringView& a, const StringView& b)
{
    return !(a == b);
}

} // namespace WTF

using WTF::StringView;

#endif
