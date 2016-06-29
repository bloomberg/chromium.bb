// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wtf/text/AtomicStringTable.h"

namespace WTF {

AtomicStringTable::AtomicStringTable()
{
    for (StringImpl* string : StringImpl::allStaticStrings().values())
        addStringImpl(string);
}

AtomicStringTable::~AtomicStringTable()
{
    for (StringImpl* string : m_table) {
        if (!string->isStatic()) {
            DCHECK(string->isAtomic());
            string->setIsAtomic(false);
        }
    }
}

StringImpl* AtomicStringTable::addStringImpl(StringImpl* string)
{
    if (!string->length())
        return StringImpl::empty();

    StringImpl* result = *m_table.add(string).storedValue;

    if (!result->isAtomic())
        result->setIsAtomic(true);

    DCHECK(!string->isStatic() || result->isStatic());
    return result;
}

} // namespace WTF
