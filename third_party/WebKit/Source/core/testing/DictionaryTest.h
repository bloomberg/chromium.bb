// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DictionaryTest_h
#define DictionaryTest_h

#include "bindings/core/v8/Nullable.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "wtf/text/WTFString.h"

namespace blink {

class InternalDictionary;

class DictionaryTest : public GarbageCollectedFinalized<DictionaryTest>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static DictionaryTest* create()
    {
        return new DictionaryTest();
    }
    virtual ~DictionaryTest();

    // Stores all members into corresponding fields
    void set(const InternalDictionary*);
    // Creates a TestDictionary instnace from fields and returns it
    InternalDictionary* get();

    void trace(Visitor*);

private:
    DictionaryTest();

    void reset();

    // The reason to use Nullable<T> is convenience; we use Nullable<T> here to
    // record whether the member field is set or not. |stringMember| isn't
    // wrapped with Nullable because |stringMember| has a non-null default value
    Nullable<unsigned> m_longMember;
    String m_stringMember;
    Nullable<bool> m_booleanOrNullMember;
    Nullable<double> m_doubleOrNullMember;
    Nullable<Vector<String> > m_stringSequenceMember;
};

} // namespace blink

#endif // DictionaryTest_h
