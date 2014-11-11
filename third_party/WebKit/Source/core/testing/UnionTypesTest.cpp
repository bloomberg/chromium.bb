// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "UnionTypesTest.h"

#include "wtf/text/StringBuilder.h"

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
    if (doubleOrString.isNull())
        return "null is passed";
    if (doubleOrString.isDouble())
        return "double is passed: " + String::numberToStringECMAScript(doubleOrString.getAsDouble());
    if (doubleOrString.isString())
        return "string is passed: " + doubleOrString.getAsString();
    ASSERT_NOT_REACHED();
    return String();
}

String UnionTypesTest::doubleOrStringArrayArg(Vector<DoubleOrString>& array)
{
    if (!array.size())
        return "";

    StringBuilder builder;
    for (DoubleOrString& doubleOrString : array) {
        ASSERT(!doubleOrString.isNull());
        if (doubleOrString.isDouble())
            builder.append("double: " + String::numberToStringECMAScript(doubleOrString.getAsDouble()));
        else if (doubleOrString.isString())
            builder.append("string: " + doubleOrString.getAsString());
        else
            ASSERT_NOT_REACHED();
        builder.append(", ");
    }
    return builder.substring(0, builder.length() - 2);
}

String UnionTypesTest::doubleOrStringSequenceArg(Vector<DoubleOrString>& sequence)
{
    return doubleOrStringArrayArg(sequence);
}

String UnionTypesTest::nodeListOrElementArg(NodeListOrElement& nodeListOrElement)
{
    ASSERT(!nodeListOrElement.isNull());
    return nodeListOrElementOrNullArg(nodeListOrElement);
}

String UnionTypesTest::nodeListOrElementOrNullArg(NodeListOrElement& nodeListOrElementOrNull)
{
    if (nodeListOrElementOrNull.isNull())
        return "null or undefined is passed";
    if (nodeListOrElementOrNull.isNodeList())
        return "nodelist is passed";
    if (nodeListOrElementOrNull.isElement())
        return "element is passed";
    ASSERT_NOT_REACHED();
    return String();
}

}
