// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "CaseMappingHarfBuzzBufferFiller.h"

#include "wtf/text/WTFString.h"

namespace blink {

static const uint16_t* toUint16(const UChar* src)
{
    // FIXME: This relies on undefined behavior however it works on the
    // current versions of all compilers we care about and avoids making
    // a copy of the string.
    static_assert(sizeof(UChar) == sizeof(uint16_t), "UChar should be the same size as uint16_t");
    return reinterpret_cast<const uint16_t*>(src);
}


CaseMappingHarfBuzzBufferFiller::CaseMappingHarfBuzzBufferFiller(
    CaseMapIntend caseMapIntend,
    hb_buffer_t* harfBuzzBuffer,
    const UChar* buffer,
    unsigned bufferLength,
    unsigned startIndex,
    unsigned numCharacters)
    : m_harfBuzzBuffer(harfBuzzBuffer)
{

    if (caseMapIntend == CaseMapIntend::KeepSameCase) {
        hb_buffer_add_utf16(m_harfBuzzBuffer,
            toUint16(buffer),
            bufferLength,
            startIndex,
            numCharacters);
    } else {
        String caseMappedText;
        if (caseMapIntend == CaseMapIntend::UpperCase) {
            caseMappedText = String(buffer, bufferLength).upper();
        } else {
            caseMappedText = String(buffer, bufferLength).lower();
        }
        // TODO(drott): crbug.com/589335 Implement those for the case where the
        // case-mapped string differs in length through one of the rule
        // references in Unicode's SpecialCasing.txt

        ASSERT(caseMappedText.length() == bufferLength);
        ASSERT(!caseMappedText.is8Bit());
        hb_buffer_add_utf16(m_harfBuzzBuffer, toUint16(caseMappedText.characters16()),
            bufferLength, startIndex, numCharacters);
    }
}

} // namespace blink
