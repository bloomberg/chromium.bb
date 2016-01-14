// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediastream/MediaDevices.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "modules/mediastream/MediaErrorState.h"
#include "modules/mediastream/MediaStream.h"
#include "modules/mediastream/MediaStreamConstraints.h"
#include "modules/mediastream/NavigatorMediaStream.h"
#include "modules/mediastream/NavigatorUserMediaErrorCallback.h"
#include "modules/mediastream/NavigatorUserMediaSuccessCallback.h"
#include "modules/mediastream/UserMediaController.h"

namespace blink {

ScriptPromise MediaDevices::enumerateDevices(ScriptState* scriptState)
{
    Document* document = toDocument(scriptState->executionContext());
    UserMediaController* userMedia = UserMediaController::from(document->frame());
    if (!userMedia)
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError, "No media device controller available; is this a detached window?"));

    MediaDevicesRequest* request = MediaDevicesRequest::create(scriptState, userMedia);
    return request->start();
}

namespace {

class PromiseSuccessCallback final : public NavigatorUserMediaSuccessCallback {
public:
    explicit PromiseSuccessCallback(ScriptPromiseResolver* resolver)
        : m_resolver(resolver)
    {
    }

    ~PromiseSuccessCallback()
    {
    }

    void handleEvent(MediaStream* stream)
    {
        m_resolver->resolve(stream);
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_resolver);
        NavigatorUserMediaSuccessCallback::trace(visitor);
    }

private:
    Member<ScriptPromiseResolver> m_resolver;
};

class PromiseErrorCallback final : public NavigatorUserMediaErrorCallback {
public:
    explicit PromiseErrorCallback(ScriptPromiseResolver* resolver)
        : m_resolver(resolver)
    {
    }

    ~PromiseErrorCallback()
    {
    }

    void handleEvent(NavigatorUserMediaError* error)
    {
        m_resolver->reject(error);
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_resolver);
        NavigatorUserMediaErrorCallback::trace(visitor);
    }

private:
    Member<ScriptPromiseResolver> m_resolver;
};

} // namespace

ScriptPromise MediaDevices::getUserMedia(ScriptState* scriptState, const MediaStreamConstraints& options, ExceptionState& exceptionState)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);

    NavigatorUserMediaSuccessCallback* successCallback = new PromiseSuccessCallback(resolver);
    NavigatorUserMediaErrorCallback* errorCallback = new PromiseErrorCallback(resolver);

    Document* document = toDocument(scriptState->executionContext());
    UserMediaController* userMedia = UserMediaController::from(document->frame());
    if (!userMedia)
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError, "No media device controller available; is this a detached window?"));

    MediaErrorState errorState;
    UserMediaRequest* request = UserMediaRequest::create(document, userMedia, options, successCallback, errorCallback, errorState);
    if (!request) {
        ASSERT(errorState.hadException());
        if (errorState.canGenerateException()) {
            errorState.raiseException(exceptionState);
            return exceptionState.reject(scriptState);
        }
        ScriptPromise rejectedPromise = resolver->promise();
        resolver->reject(errorState.createError());
        return rejectedPromise;
    }

    String errorMessage;
    if (!request->isSecureContextUse(errorMessage)) {
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError, errorMessage));
    }

    request->start();
    return resolver->promise();
}

} // namespace blink
