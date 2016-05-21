// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementDescriptor_h
#define CustomElementDescriptor_h

#include "core/CoreExport.h"
#include "wtf/Allocator.h"
#include "wtf/HashTableDeletedValueType.h"
#include "wtf/text/AtomicString.h"

namespace blink {

// Describes what elements a custom element definition applies to.
//
// There are two kinds of definitions: The first has its own tag name;
// in this case the "name" (definition name) and local name (tag name)
// are the same. The second kind customizes a built-in element; in
// that case, the descriptor's local name will be a built-in element
// name, or an unknown element name that is *not* a valid custom
// element name.
//
// This type is used when the kind of custom element definition is
// known, and generally the difference is important. For example, a
// definition for "my-element", "my-element" must not be applied to an
// element <button is="my-element">.
class CORE_EXPORT CustomElementDescriptor final {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
public:
    CustomElementDescriptor()
    {
    }

    CustomElementDescriptor(
        const AtomicString& name,
        const AtomicString& localName)
        : m_name(name)
        , m_localName(localName)
    {
    }

    explicit CustomElementDescriptor(WTF::HashTableDeletedValueType value)
        : m_name(value)
    {
    }

    bool isHashTableDeletedValue() const
    {
        return m_name.isHashTableDeletedValue();
    }

    bool operator==(const CustomElementDescriptor& other) const
    {
        return m_name == other.m_name && m_localName == other.m_localName;
    }

    const AtomicString& name() const { return m_name; }
    const AtomicString& localName() const { return m_localName; }

private:
    AtomicString m_name;
    AtomicString m_localName;
};

} // namespace blink

#endif // CustomElementDescriptor_h
