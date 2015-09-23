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
    ReadableByteStream* body();
    virtual BodyStreamBuffer* bodyBuffer() = 0;
    virtual const BodyStreamBuffer* bodyBuffer() const = 0;

    bool bodyUsed();
    void setBodyPassed() { m_bodyPassed = true; }

    // ActiveDOMObject override.
    bool hasPendingActivity() const override;

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        ActiveDOMObject::trace(visitor);
    }

    void makeOpaque() { m_opaque = true; }
    bool opaque() const { return m_opaque; }

private:
    virtual String mimeType() const = 0;

    bool m_bodyPassed;
    bool m_opaque;
};

} // namespace blink

#endif // Body_h
