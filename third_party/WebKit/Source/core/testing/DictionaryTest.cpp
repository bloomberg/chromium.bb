// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "DictionaryTest.h"

#include "core/testing/InternalDictionary.h"

namespace blink {

DictionaryTest::DictionaryTest()
{
}

DictionaryTest::~DictionaryTest()
{
}

void DictionaryTest::set(const InternalDictionary& testingDictionary)
{
    reset();
    if (testingDictionary.hasLongMember())
        m_longMember = testingDictionary.longMember();
    m_longMemberWithDefault = testingDictionary.longMemberWithDefault();
    if (testingDictionary.hasLongOrNullMember())
        m_longOrNullMember = testingDictionary.longOrNullMember();
    // |longOrNullMemberWithDefault| has a default value but can be null, so
    // we need to check availability.
    if (testingDictionary.hasLongOrNullMemberWithDefault())
        m_longOrNullMemberWithDefault = testingDictionary.longOrNullMemberWithDefault();
    if (testingDictionary.hasBooleanMember())
        m_booleanMember = testingDictionary.booleanMember();
    if (testingDictionary.hasDoubleMember())
        m_doubleMember = testingDictionary.doubleMember();
    m_stringMember = testingDictionary.stringMember();
    m_stringMemberWithDefault = testingDictionary.stringMemberWithDefault();
    if (testingDictionary.hasStringSequenceMember())
        m_stringSequenceMember = testingDictionary.stringSequenceMember();
    if (testingDictionary.hasStringSequenceOrNullMember())
        m_stringSequenceOrNullMember = testingDictionary.stringSequenceOrNullMember();
    m_enumMember = testingDictionary.enumMember();
    m_enumMemberWithDefault = testingDictionary.enumMemberWithDefault();
    m_enumOrNullMember = testingDictionary.enumOrNullMember();
    if (testingDictionary.hasElementMember())
        m_elementMember = testingDictionary.elementMember();
    if (testingDictionary.hasElementOrNullMember())
        m_elementOrNullMember = testingDictionary.elementOrNullMember();
    m_objectMember = testingDictionary.objectMember();
    m_objectOrNullMemberWithDefault = testingDictionary.objectOrNullMemberWithDefault();
}

InternalDictionary* DictionaryTest::get()
{
    InternalDictionary* result = InternalDictionary::create();
    if (m_longMember)
        result->setLongMember(m_longMember.get());
    result->setLongMemberWithDefault(m_longMemberWithDefault);
    if (m_longOrNullMember)
        result->setLongOrNullMember(m_longOrNullMember.get());
    if (m_longOrNullMemberWithDefault)
        result->setLongOrNullMemberWithDefault(m_longOrNullMemberWithDefault.get());
    if (m_booleanMember)
        result->setBooleanMember(m_booleanMember.get());
    if (m_doubleMember)
        result->setDoubleMember(m_doubleMember.get());
    result->setStringMember(m_stringMember);
    result->setStringMemberWithDefault(m_stringMemberWithDefault);
    if (m_stringSequenceMember)
        result->setStringSequenceMember(m_stringSequenceMember.get());
    if (m_stringSequenceOrNullMember)
        result->setStringSequenceOrNullMember(m_stringSequenceOrNullMember.get());
    result->setEnumMember(m_enumMember);
    result->setEnumMemberWithDefault(m_enumMemberWithDefault);
    result->setEnumOrNullMember(m_enumOrNullMember);
    if (m_elementMember)
        result->setElementMember(m_elementMember);
    if (m_elementOrNullMember)
        result->setElementOrNullMember(m_elementOrNullMember);
    result->setObjectMember(m_objectMember);
    result->setObjectOrNullMemberWithDefault(m_objectOrNullMemberWithDefault);
    return result;
}

void DictionaryTest::reset()
{
    m_longMember = Nullable<int>();
    m_longMemberWithDefault = -1; // This value should not be returned.
    m_longOrNullMember = Nullable<int>();
    m_longOrNullMemberWithDefault = Nullable<int>();
    m_booleanMember = Nullable<bool>();
    m_doubleMember = Nullable<double>();
    m_stringMember = String();
    m_stringMemberWithDefault = String("Should not be returned");
    m_stringSequenceMember = Nullable<Vector<String> >();
    m_stringSequenceOrNullMember = Nullable<Vector<String> >();
    m_enumMember = String();
    m_enumMemberWithDefault = String();
    m_enumOrNullMember = String();
    m_elementMember = nullptr;
    m_elementOrNullMember = nullptr;
    m_objectMember = ScriptValue();
    m_objectOrNullMemberWithDefault = ScriptValue();
}

void DictionaryTest::trace(Visitor* visitor)
{
    visitor->trace(m_elementMember);
    visitor->trace(m_elementOrNullMember);
}

}
