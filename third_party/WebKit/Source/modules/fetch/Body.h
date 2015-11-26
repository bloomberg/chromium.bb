// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Body_h
#define Body_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"
#include "wtf/text/WTFString.h"

namespace blink {

class BodyStreamBuffer;
class ExecutionContext;
class ReadableByteStream;
class ScriptState;

// This class represents Body mix-in defined in the fetch spec
// https://fetch.spec.whatwg.org/#body-mixin.
//
// Note: This class has body stream and its predicate whereas in the current
// spec only Response has it and Request has a byte stream defined in the
// Encoding spec. The spec should be fixed shortly to be aligned with this
// implementation.
class MODULES_EXPORT Body
    : public GarbageCollectedFinalized<Body>
    , public ScriptWrappable
    , public ActiveDOMObject {
    WTF_MAKE_NONCOPYABLE(Body);
    DEFINE_WRAPPERTYPEINFO();
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(Body);
public:
    explicit Body(ExecutionContext*);
    ~Body() override { }

    ScriptPromise arrayBuffer(ScriptState*);
    ScriptPromise blob(ScriptState*);
    ScriptPromise formData(ScriptState*);
    ScriptPromise json(ScriptState*);
    ScriptPromise text(ScriptState*);
    ReadableByteStream* bodyWithUseCounter();
    virtual BodyStreamBuffer* bodyBuffer() = 0;
    virtual const BodyStreamBuffer* bodyBuffer() const = 0;

    virtual bool bodyUsed();
    bool isBodyLocked();

    // ActiveDOMObject override.
    bool hasPendingActivity() const override;

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        ActiveDOMObject::trace(visitor);
    }

    // https://w3c.github.io/webappsec-credential-management/#monkey-patching-fetch-2
    void setOpaque() { m_opaque = true; }
    bool opaque() const { return m_opaque; }

private:
    ReadableByteStream* body();
    virtual String mimeType() const = 0;

    // Body consumption algorithms will reject with a TypeError in a number of
    // error conditions. This method wraps those up into one call which returns
    // an empty ScriptPromise if the consumption may proceed, and a
    // ScriptPromise rejected with a TypeError if it ought to be blocked.
    ScriptPromise rejectInvalidConsumption(ScriptState*);

    bool m_opaque;
};

} // namespace blink

#endif // Body_h
