// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WTF_AtomicStringTable_h
#define WTF_AtomicStringTable_h

#include "wtf/Allocator.h"
#include "wtf/HashSet.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/StringImpl.h"

namespace WTF {

// The underlying storage that keeps the map of unique AtomicStrings. This is
// not thread safe and each WTFThreadData has one.
class AtomicStringTable {
    USING_FAST_MALLOC(AtomicStringTable);
    WTF_MAKE_NONCOPYABLE(AtomicStringTable);
public:
    AtomicStringTable();
    ~AtomicStringTable();

    StringImpl* addStringImpl(StringImpl*);

    HashSet<StringImpl*>& table() { return m_table; }

private:
    HashSet<StringImpl*> m_table;
};

} // namespace WTF

#endif
