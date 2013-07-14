/*
 * Copyright (C) 2013 Google, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/html/parser/HTMLIdentifier.h"

#include "HTMLNames.h"
#include "wtf/HashMap.h"
#include "wtf/MainThread.h"
#include "wtf/text/StringHash.h"

namespace WebCore {

using namespace HTMLNames;

typedef HashMap<unsigned, StringImpl*, AlreadyHashed> IdentifierTable;

unsigned HTMLIdentifier::maxNameLength = 0;

static IdentifierTable& identifierTable()
{
    DEFINE_STATIC_LOCAL(IdentifierTable, table, ());
    ASSERT(isMainThread() || !table.isEmpty());
    return table;
}

#ifndef NDEBUG
bool HTMLIdentifier::isKnown(const StringImpl* string)
{
    const IdentifierTable& table = identifierTable();
    return table.contains(string->hash());
}
#endif

StringImpl* HTMLIdentifier::findIfKnown(const UChar* characters, unsigned length)
{
    // We don't need to try hashing if we know the string is too long.
    if (length > maxNameLength)
        return 0;
    // computeHashAndMaskTop8Bits is the function StringImpl::hash() uses.
    unsigned hash = StringHasher::computeHashAndMaskTop8Bits(characters, length);
    const IdentifierTable& table = identifierTable();
    ASSERT(!table.isEmpty());

    IdentifierTable::const_iterator it = table.find(hash);
    if (it == table.end())
        return 0;
    // It's possible to have hash collisions between arbitrary strings and
    // known identifiers (e.g. "bvvfg" collides with "script").
    // However ASSERTs in addNames() guard against there ever being collisions
    // between known identifiers.
    if (!equal(it->value, characters, length))
        return 0;
    return it->value;
}

const unsigned kHTMLNamesIndexOffset = 0;
const unsigned kHTMLAttrsIndexOffset = 1000;
COMPILE_ASSERT(kHTMLAttrsIndexOffset > HTMLTagsCount, kHTMLAttrsIndexOffset_should_be_larger_than_HTMLTagsCount);

const String& HTMLIdentifier::asString() const
{
    ASSERT(isMainThread());
    return m_string;
}

const StringImpl* HTMLIdentifier::asStringImpl() const
{
    return m_string.impl();
}

void HTMLIdentifier::addNames(QualifiedName** names, unsigned namesCount, unsigned indexOffset)
{
    IdentifierTable& table = identifierTable();
    for (unsigned i = 0; i < namesCount; ++i) {
        StringImpl* name = names[i]->localName().impl();
        unsigned hash = name->hash();
        IdentifierTable::AddResult addResult = table.add(hash, name);
        maxNameLength = std::max(maxNameLength, name->length());
        // Ensure we're using the same hashing algorithm to get and set.
        ASSERT_UNUSED(addResult, !addResult.isNewEntry || HTMLIdentifier::findIfKnown(String(name).charactersWithNullTermination().data(), name->length()) == name);
        // We expect some hash collisions, but only for identical strings.
        // Since all of these names are AtomicStrings pointers should be equal.
        // Note: If you hit this ASSERT, then we had a hash collision among
        // HTMLNames strings, and we need to re-design how we use this hash!
        ASSERT_UNUSED(addResult, !addResult.isNewEntry || name == addResult.iterator->value);
    }
}

void HTMLIdentifier::init()
{
    ASSERT(isMainThread()); // Not technically necessary, but this is our current expected usage.
    static bool isInitialized = false;
    if (isInitialized)
        return;
    isInitialized = true;

    // FIXME: We should atomize small whitespace (\n, \n\n, etc.)
    addNames(getHTMLTags(), HTMLTagsCount, kHTMLNamesIndexOffset);
    addNames(getHTMLAttrs(), HTMLAttrsCount, kHTMLAttrsIndexOffset);
}

}
