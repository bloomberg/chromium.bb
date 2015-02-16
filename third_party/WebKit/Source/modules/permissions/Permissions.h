// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Permissions_h
#define Permissions_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace WTF {
class AtomicString;
} // namespace WTF

namespace blink {

class ScriptState;

class Permissions final
    : public GarbageCollectedFinalized<Permissions>
    , public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    virtual ~Permissions();

    DECLARE_VIRTUAL_TRACE();

    static ScriptPromise query(ScriptState*, const AtomicString&);
};

} // namespace blink

#endif // Permissions_h
