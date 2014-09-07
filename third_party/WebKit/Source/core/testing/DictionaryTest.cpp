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

void DictionaryTest::set(const InternalDictionary* testingDictionary)
{
    reset();
    if (testingDictionary->hasLongMember())
        m_longMember = testingDictionary->longMember();
    m_stringMember = testingDictionary->stringMember();
    if (testingDictionary->hasBooleanOrNullMember())
        m_booleanOrNullMember = testingDictionary->booleanOrNullMember();
    if (testingDictionary->hasDoubleOrNullMember())
        m_doubleOrNullMember = testingDictionary->doubleOrNullMember();
    if (testingDictionary->hasStringSequenceMember())
        m_stringSequenceMember = testingDictionary->stringSequenceMember();
}

InternalDictionary* DictionaryTest::get()
{
    InternalDictionary* result = InternalDictionary::create();
    if (m_longMember)
        result->setLongMember(m_longMember.get());
    result->setStringMember(m_stringMember);
    if (m_booleanOrNullMember)
        result->setBooleanOrNullMember(m_booleanOrNullMember.get());
    if (m_doubleOrNullMember)
        result->setDoubleOrNullMember(m_doubleOrNullMember.get());
    if (m_stringSequenceMember)
        result->setStringSequenceMember(m_stringSequenceMember.get());
    return result;
}

void DictionaryTest::reset()
{
    m_longMember = Nullable<unsigned>();
    m_stringMember = String();
    m_booleanOrNullMember = Nullable<bool>();
    m_doubleOrNullMember = Nullable<double>();
    m_stringSequenceMember = Nullable<Vector<String> >();
}

void DictionaryTest::trace(Visitor*)
{
}

}
