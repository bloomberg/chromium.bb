// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UnionTypesTest_h
#define UnionTypesTest_h

#include "bindings/core/v8/UnionTypesCore.h"
#include "wtf/text/WTFString.h"

namespace blink {

class UnionTypesTest final : public GarbageCollectedFinalized<UnionTypesTest>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static UnionTypesTest* create()
    {
        return new UnionTypesTest();
    }
    virtual ~UnionTypesTest() { }

    String doubleOrStringArg(DoubleOrString&);

    void trace(Visitor*) { }

private:
    UnionTypesTest() { }
};

} // namespace blink

#endif // UnionTypesTest_h
