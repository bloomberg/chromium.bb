// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DictionaryTest_h
#define DictionaryTest_h

#include "bindings/core/v8/Nullable.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/Element.h"
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
    void set(const InternalDictionary&);
    // Creates a TestDictionary instnace from fields and returns it
    InternalDictionary* get();

    void trace(Visitor*);

private:
    DictionaryTest();

    void reset();

    // The reason to use Nullable<T> is convenience; we use Nullable<T> here to
    // record whether the member field is set or not. Some members are not
    // wrapped with Nullable because:
    //  - |longMemberWithDefault| has a non-null default value
    //  - String and PtrTypes can express whether they are null
    Nullable<int> m_longMember;
    int m_longMemberWithDefault;
    Nullable<int> m_longOrNullMember;
    Nullable<int> m_longOrNullMemberWithDefault;
    Nullable<bool> m_booleanMember;
    Nullable<double> m_doubleMember;
    String m_stringMember;
    String m_stringMemberWithDefault;
    Nullable<Vector<String> > m_stringSequenceMember;
    Nullable<Vector<String> > m_stringSequenceOrNullMember;
    String m_enumMember;
    String m_enumMemberWithDefault;
    String m_enumOrNullMember;
    RefPtrWillBeMember<Element> m_elementMember;
    RefPtrWillBeMember<Element> m_elementOrNullMember;
};

} // namespace blink

#endif // DictionaryTest_h
