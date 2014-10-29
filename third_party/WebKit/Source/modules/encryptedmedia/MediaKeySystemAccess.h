// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaKeySystemAccess_h
#define MediaKeySystemAccess_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "wtf/text/WTFString.h"

namespace blink {

class MediaKeySystemAccess final : public GarbageCollectedFinalized<MediaKeySystemAccess>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();

public:
    explicit MediaKeySystemAccess(const String& keySystem);
    virtual ~MediaKeySystemAccess();

    const String& keySystem() const { return m_keySystem; }
    ScriptPromise createMediaKeys(ScriptState*);

    void trace(Visitor*);

private:
    const String m_keySystem;
};

} // namespace blink

#endif // MediaKeySystemAccess_h
