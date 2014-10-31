// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "UnionTypesTest.h"

namespace blink {

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
