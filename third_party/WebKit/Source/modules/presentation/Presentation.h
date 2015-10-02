// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Presentation_h
#define Presentation_h

#include "bindings/core/v8/ScriptPromise.h"
#include "core/events/EventTarget.h"
#include "core/frame/DOMWindowProperty.h"
#include "modules/presentation/PresentationConnection.h"
#include "platform/heap/Handle.h"
#include "platform/heap/Heap.h"

namespace WTF {
class String;
} // namespace WTF

namespace blink {

class LocalFrame;
class PresentationController;
class PresentationRequest;
class ScriptState;
class WebPresentationConnectionClient;
enum class WebPresentationConnectionState;

// Implements the main entry point of the Presentation API corresponding to the Presentation.idl
// See https://w3c.github.io/presentation-api/#navigatorpresentation for details.
class Presentation final
    : public RefCountedGarbageCollectedEventTargetWithInlineData<Presentation>
    , public DOMWindowProperty {
    REFCOUNTED_GARBAGE_COLLECTED_EVENT_TARGET(Presentation);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(Presentation);
    DEFINE_WRAPPERTYPEINFO();
public:
    static Presentation* create(LocalFrame*);
    ~Presentation() override = default;

    // EventTarget implementation.
    const AtomicString& interfaceName() const override;
    ExecutionContext* executionContext() const override;

    DECLARE_VIRTUAL_TRACE();

    PresentationRequest* defaultRequest() const;
    void setDefaultRequest(PresentationRequest*);

    PresentationConnection* connection() const;

private:
    explicit Presentation(LocalFrame*);

    // The connection object provided to the presentation page. Not supported.
    Member<PresentationConnection> m_connection;

    // Default PresentationRequest used by the embedder.
    Member<PresentationRequest> m_defaultRequest;
};

} // namespace blink

#endif // Presentation_h
