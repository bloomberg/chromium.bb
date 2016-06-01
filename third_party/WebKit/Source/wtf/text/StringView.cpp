// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wtf/text/StringView.h"

namespace WTF {

StringView::StringView(const UChar* chars, unsigned length)
    : m_length(length)
    , m_is8Bit(false)
{
    m_data.characters16 = chars;
}

StringView::StringView(const UChar* chars)
    : StringView(chars, chars ? lengthOfNullTerminatedString(chars) : 0) {}

#if DCHECK_IS_ON()
StringView::~StringView()
{
    // StringView does not own the StringImpl, we must not be the last ref.
    DCHECK(!m_impl || !m_impl->hasOneRef());
}
#endif

String StringView::toString() const
{
    if (isNull())
        return String();
    if (isEmpty())
        return emptyString();
    if (is8Bit())
        return String(m_data.characters8, m_length);
    return StringImpl::create8BitIfPossible(m_data.characters16, m_length);
}

bool equalStringView(const StringView& a, const StringView& b)
{
    if (a.isNull() || b.isNull())
        return a.isNull() == b.isNull();
    if (a.length() != b.length())
        return false;
    if (a.is8Bit()) {
        if (b.is8Bit())
            return equal(a.characters8(), b.characters8(), a.length());
        return equal(a.characters8(), b.characters16(), a.length());
    }
    if (b.is8Bit())
        return equal(a.characters16(), b.characters8(), a.length());
    return equal(a.characters16(), b.characters16(), a.length());
}

bool equalIgnoringASCIICase(const StringView& a, const StringView& b)
{
    if (a.isNull() || b.isNull())
        return a.isNull() == b.isNull();
    if (a.length() != b.length())
        return false;
    if (a.is8Bit()) {
        if (b.is8Bit())
            return equalIgnoringASCIICase(a.characters8(), b.characters8(), a.length());
        return equalIgnoringASCIICase(a.characters8(), b.characters16(), a.length());
    }
    if (b.is8Bit())
        return equalIgnoringASCIICase(a.characters16(), b.characters8(), a.length());
    return equalIgnoringASCIICase(a.characters16(), b.characters16(), a.length());
}

} // namespace WTF
