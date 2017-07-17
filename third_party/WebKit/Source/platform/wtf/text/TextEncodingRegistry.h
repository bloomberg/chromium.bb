/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TextEncodingRegistry_h
#define TextEncodingRegistry_h

#include "platform/wtf/WTFExport.h"
#include "platform/wtf/text/Unicode.h"
#include "platform/wtf/text/WTFString.h"
#include <memory>

namespace WTF {

class TextCodec;
class TextEncoding;

// Use TextResourceDecoder::decode to decode resources, since it handles BOMs.
// Use TextEncoding::encode to encode, since it takes care of normalization.
WTF_EXPORT std::unique_ptr<TextCodec> NewTextCodec(const TextEncoding&);

// Only TextEncoding should use the following functions directly.
const char* AtomicCanonicalTextEncodingName(const char* alias);
template <typename CharacterType>
const char* AtomicCanonicalTextEncodingName(const CharacterType*, size_t);
const char* AtomicCanonicalTextEncodingName(const String&);
bool NoExtendedTextEncodingNameUsed();

#ifndef NDEBUG
void DumpTextEncodingNameMap();
#endif

}  // namespace WTF

using WTF::NewTextCodec;
using WTF::AtomicCanonicalTextEncodingName;
using WTF::NoExtendedTextEncodingNameUsed;
#ifndef NDEBUG
using WTF::DumpTextEncodingNameMap;
#endif

#endif  // TextEncodingRegistry_h
