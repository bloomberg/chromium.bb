/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AttributeCollection_h
#define AttributeCollection_h

#include "core/dom/Attribute.h"

namespace blink {

class Attr;

class AttributeCollection {
public:
    typedef const Attribute* const_iterator;

    AttributeCollection(const Attribute* array, unsigned size)
        : m_array(array)
        , m_size(size)
    { }

    const Attribute& operator[](unsigned index) const { return at(index); }
    const Attribute& at(unsigned index) const
    {
        RELEASE_ASSERT(index < m_size);
        return m_array[index];
    }

    const Attribute* find(const QualifiedName&) const;
    const Attribute* find(const AtomicString& name, bool shouldIgnoreCase) const;
    size_t findIndex(const QualifiedName&, bool shouldIgnoreCase = false) const;
    size_t findIndex(const AtomicString& name, bool shouldIgnoreCase) const;
    size_t findIndex(Attr*) const;

    const_iterator begin() const { return m_array; }
    const_iterator end() const { return m_array + m_size; }

    unsigned size() const { return m_size; }
    bool isEmpty() const { return !m_size; }

private:
    size_t findSlowCase(const AtomicString& name, bool shouldIgnoreAttributeCase) const;

    const Attribute* m_array;
    unsigned m_size;
};

inline const Attribute* AttributeCollection::find(const AtomicString& name, bool shouldIgnoreCase) const
{
    size_t index = findIndex(name, shouldIgnoreCase);
    return index != kNotFound ? &at(index) : 0;
}

inline size_t AttributeCollection::findIndex(const QualifiedName& name, bool shouldIgnoreCase) const
{
    const_iterator end = this->end();
    unsigned index = 0;
    for (const_iterator it = begin(); it != end; ++it, ++index) {
        if (it->name().matchesPossiblyIgnoringCase(name, shouldIgnoreCase))
            return index;
    }
    return kNotFound;
}

// We use a boolean parameter instead of calling shouldIgnoreAttributeCase so that the caller
// can tune the behavior (hasAttribute is case sensitive whereas getAttribute is not).
inline size_t AttributeCollection::findIndex(const AtomicString& name, bool shouldIgnoreCase) const
{
    bool doSlowCheck = shouldIgnoreCase;

    // Optimize for the case where the attribute exists and its name exactly matches.
    const_iterator end = this->end();
    unsigned index = 0;
    for (const_iterator it = begin(); it != end; ++it, ++index) {
        // FIXME: Why check the prefix? Namespaces should be all that matter.
        // Most attributes (all of HTML and CSS) have no namespace.
        if (!it->name().hasPrefix()) {
            if (name == it->localName())
                return index;
        } else {
            doSlowCheck = true;
        }
    }

    if (doSlowCheck)
        return findSlowCase(name, shouldIgnoreCase);
    return kNotFound;
}

inline const Attribute* AttributeCollection::find(const QualifiedName& name) const
{
    const_iterator end = this->end();
    for (const_iterator it = begin(); it != end; ++it) {
        if (it->name().matches(name))
            return it;
    }
    return 0;
}

} // namespace blink

#endif // AttributeCollection_h
