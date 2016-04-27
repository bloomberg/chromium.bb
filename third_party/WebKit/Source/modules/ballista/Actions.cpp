// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/ballista/Actions.h"

#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/LocalFrame.h"
#include "platform/mojo/MojoHelper.h"
#include "public/platform/Platform.h"
#include "public/platform/ServiceRegistry.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

class Actions::BallistaClientImpl : public GarbageCollectedFinalized<BallistaClientImpl> {
public:
    BallistaClientImpl(Actions*, ScriptPromiseResolver*);

    void callback(const String& error);

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_actions);
        visitor->trace(m_resolver);
    }

private:
    WeakMember<Actions> m_actions;
    Member<ScriptPromiseResolver> m_resolver;
};

Actions::BallistaClientImpl::BallistaClientImpl(Actions* actions, ScriptPromiseResolver* resolver)
    : m_actions(actions)
    , m_resolver(resolver)
{
}

void Actions::BallistaClientImpl::callback(const String& error)
{
    if (m_actions)
        m_actions->m_clients.remove(this);

    if (error.isNull()) {
        m_resolver->resolve();
    } else {
        // TODO(mgiuca): Work out which error type to use.
        m_resolver->reject(DOMException::create(AbortError, error));
    }
}

Actions::~Actions() = default;

ScriptPromise Actions::share(ScriptState* scriptState, const String& title, const String& text)
{
    if (!m_service) {
        Document* doc = toDocument(scriptState->getExecutionContext());
        DCHECK(doc);
        LocalFrame* frame = doc->frame();
        DCHECK(frame);
        frame->serviceRegistry()->connectToRemoteService(mojo::GetProxy(&m_service));
        DCHECK(m_service);
    }

    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    BallistaClientImpl* client = new BallistaClientImpl(this, resolver);
    m_clients.add(client);
    ScriptPromise promise = resolver->promise();

    m_service->Share(title, text, sameThreadBindForMojo(&BallistaClientImpl::callback, client));

    return promise;
}

DEFINE_TRACE(Actions)
{
    visitor->trace(m_clients);
}

Actions::Actions()
{
}

} // namespace blink
