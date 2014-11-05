// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "UnionTypesTest.h"

namespace blink {

void UnionTypesTest::doubleOrStringAttribute(DoubleOrString& doubleOrString)
{
    switch (m_attributeType) {
    case SpecificTypeNone:
        // Default value is zero (of double).
        doubleOrString.setDouble(0);
        break;
    case SpecificTypeDouble:
        doubleOrString.setDouble(m_attributeDouble);
        break;
    case SpecificTypeString:
        doubleOrString.setString(m_attributeString);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

void UnionTypesTest::setDoubleOrStringAttribute(const DoubleOrString& doubleOrString)
{
    if (doubleOrString.isDouble()) {
        m_attributeDouble = doubleOrString.getAsDouble();
        m_attributeType = SpecificTypeDouble;
    } else if (doubleOrString.isString()) {
        m_attributeString = doubleOrString.getAsString();
        m_attributeType = SpecificTypeString;
    } else {
        ASSERT_NOT_REACHED();
    }
}

String UnionTypesTest::doubleOrStringArg(DoubleOrString& doubleOrString)
{
    ASSERT(!doubleOrString.isNull());
    if (doubleOrString.isDouble())
        return "double is passed: " + String::numberToStringECMAScript(doubleOrString.getAsDouble());
    if (doubleOrString.isString())
        return "string is passed: " + doubleOrString.getAsString();
    ASSERT_NOT_REACHED();
    return String();
}

}
