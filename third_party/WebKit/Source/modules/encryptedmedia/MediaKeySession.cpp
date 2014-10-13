/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "modules/encryptedmedia/MediaKeySession.h"

#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/ExceptionCode.h"
#include "core/events/Event.h"
#include "core/events/GenericEventQueue.h"
#include "core/html/MediaKeyError.h"
#include "modules/encryptedmedia/MediaKeyMessageEvent.h"
#include "modules/encryptedmedia/MediaKeys.h"
#include "modules/encryptedmedia/SimpleContentDecryptionModuleResult.h"
#include "platform/ContentDecryptionModuleResult.h"
#include "platform/ContentType.h"
#include "platform/Logging.h"
#include "platform/MIMETypeRegistry.h"
#include "platform/Timer.h"
#include "public/platform/WebContentDecryptionModule.h"
#include "public/platform/WebContentDecryptionModuleException.h"
#include "public/platform/WebContentDecryptionModuleSession.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "wtf/ArrayBuffer.h"
#include "wtf/ArrayBufferView.h"

namespace blink {

static bool isKeySystemSupportedWithInitDataType(const String& keySystem, const String& initDataType)
{
    ASSERT(!keySystem.isEmpty());

    // FIXME: initDataType != contentType. Implement this properly.
    // http://crbug.com/385874.
    String contentType = initDataType;
    if (initDataType == "webm") {
        contentType = "video/webm";
    } else if (initDataType == "cenc") {
        contentType = "video/mp4";
    }

    ContentType type(contentType);
    return MIMETypeRegistry::isSupportedEncryptedMediaMIMEType(keySystem, type.type(), type.parameter("codecs"));
}

// A class holding a pending action.
class MediaKeySession::PendingAction : public GarbageCollectedFinalized<MediaKeySession::PendingAction> {
public:
    enum Type {
        GenerateRequest,
        Update,
        Release
    };

    Type type() const { return m_type; }

    const Persistent<ContentDecryptionModuleResult> result() const
    {
        return m_result;
    }

    const RefPtr<ArrayBuffer> data() const
    {
        ASSERT(m_type == GenerateRequest || m_type == Update);
        return m_data;
    }

    const String& initDataType() const
    {
        ASSERT(m_type == GenerateRequest);
        return m_initDataType;
    }

    static PendingAction* CreatePendingGenerateRequest(ContentDecryptionModuleResult* result, const String& initDataType, PassRefPtr<ArrayBuffer> initData)
    {
        ASSERT(result);
        ASSERT(initData);
        return new PendingAction(GenerateRequest, result, initDataType, initData);
    }

    static PendingAction* CreatePendingUpdate(ContentDecryptionModuleResult* result, PassRefPtr<ArrayBuffer> data)
    {
        ASSERT(result);
        ASSERT(data);
        return new PendingAction(Update, result, String(), data);
    }

    static PendingAction* CreatePendingRelease(ContentDecryptionModuleResult* result)
    {
        ASSERT(result);
        return new PendingAction(Release, result, String(), PassRefPtr<ArrayBuffer>());
    }

    ~PendingAction()
    {
    }

    void trace(Visitor* visitor)
    {
        visitor->trace(m_result);
    }

private:
    PendingAction(Type type, ContentDecryptionModuleResult* result, const String& initDataType, PassRefPtr<ArrayBuffer> data)
        : m_type(type)
        , m_result(result)
        , m_initDataType(initDataType)
        , m_data(data)
    {
    }

    const Type m_type;
    const Member<ContentDecryptionModuleResult> m_result;
    const String m_initDataType;
    const RefPtr<ArrayBuffer> m_data;
};

// This class wraps the promise resolver used when initializing a new session
// and is passed to Chromium to fullfill the promise. This implementation of
// completeWithSession() will resolve the promise with void, while
// completeWithError() will reject the promise with an exception. complete()
// is not expected to be called, and will reject the promise.
class NewSessionResult : public ContentDecryptionModuleResult {
public:
    NewSessionResult(ScriptState* scriptState, MediaKeySession* session)
        : m_resolver(ScriptPromiseResolver::create(scriptState))
        , m_session(session)
    {
        WTF_LOG(Media, "NewSessionResult(%p)", this);
    }

    virtual ~NewSessionResult()
    {
        WTF_LOG(Media, "~NewSessionResult(%p)", this);
    }

    // ContentDecryptionModuleResult implementation.
    virtual void complete() override
    {
        ASSERT_NOT_REACHED();
        completeWithDOMException(InvalidStateError, "Unexpected completion.");
    }

    virtual void completeWithSession(WebContentDecryptionModuleResult::SessionStatus status) override
    {
        if (status != WebContentDecryptionModuleResult::NewSession) {
            ASSERT_NOT_REACHED();
            completeWithDOMException(InvalidStateError, "Unexpected completion.");
        }

        m_session->finishGenerateRequest();
        m_resolver->resolve();
        m_resolver.clear();
    }

    virtual void completeWithError(WebContentDecryptionModuleException exceptionCode, unsigned long systemCode, const WebString& errorMessage) override
    {
        completeWithDOMException(WebCdmExceptionToExceptionCode(exceptionCode), errorMessage);
    }

    // It is only valid to call this before completion.
    ScriptPromise promise() { return m_resolver->promise(); }

    void trace(Visitor* visitor)
    {
        visitor->trace(m_session);
        ContentDecryptionModuleResult::trace(visitor);
    }

private:
    // Reject the promise with a DOMException.
    void completeWithDOMException(ExceptionCode code, const String& errorMessage)
    {
        m_resolver->reject(DOMException::create(code, errorMessage));
        m_resolver.clear();
    }

    RefPtr<ScriptPromiseResolver> m_resolver;
    Member<MediaKeySession> m_session;
};

MediaKeySession* MediaKeySession::create(ScriptState* scriptState, MediaKeys* mediaKeys, const String& sessionType)
{
    RefPtrWillBeRawPtr<MediaKeySession> session = new MediaKeySession(scriptState, mediaKeys, sessionType);
    session->suspendIfNeeded();
    return session.get();
}

MediaKeySession::MediaKeySession(ScriptState* scriptState, MediaKeys* mediaKeys, const String& sessionType)
    : ActiveDOMObject(scriptState->executionContext())
    , m_keySystem(mediaKeys->keySystem())
    , m_asyncEventQueue(GenericEventQueue::create(this))
    , m_mediaKeys(mediaKeys)
    , m_sessionType(sessionType)
    , m_isUninitialized(true)
    , m_isCallable(false)
    , m_isClosed(false)
    , m_closedPromise(new ClosedPromise(scriptState->executionContext(), this, ClosedPromise::Closed))
    , m_actionTimer(this, &MediaKeySession::actionTimerFired)
{
    WTF_LOG(Media, "MediaKeySession(%p)::MediaKeySession", this);

    // Create the matching Chromium object. It will not be usable until
    // initializeNewSession() is called in response to the user calling
    // generateRequest().
    WebContentDecryptionModule* cdm = mediaKeys->contentDecryptionModule();
    m_session = adoptPtr(cdm->createSession());
    m_session->setClientInterface(this);

    // MediaKeys::createSession(), step 2.
    // 2.1 Let the sessionId attribute be the empty string.
    ASSERT(sessionId().isEmpty());

    // 2.2 Let the expiration attribute be NaN.
    // FIXME: Add expiration property.

    // 2.3 Let the closed attribute be a new promise.
    ASSERT(!closed(scriptState).isUndefinedOrNull());

    // 2.4 Let the session type be sessionType.
    ASSERT(sessionType == m_sessionType);

    // 2.5 Let uninitialized be true.
    ASSERT(m_isUninitialized);

    // 2.6 Let callable be false.
    ASSERT(!m_isCallable);
}

MediaKeySession::~MediaKeySession()
{
    WTF_LOG(Media, "MediaKeySession(%p)::~MediaKeySession", this);
    m_session.clear();
#if !ENABLE(OILPAN)
    // MediaKeySession and m_asyncEventQueue always become unreachable
    // together. So MediaKeySession and m_asyncEventQueue are destructed in the
    // same GC. We don't need to call cancelAllEvents explicitly in Oilpan.
    m_asyncEventQueue->cancelAllEvents();
#endif
}

void MediaKeySession::setError(MediaKeyError* error)
{
    m_error = error;
}

String MediaKeySession::sessionId() const
{
    return m_session->sessionId();
}

ScriptPromise MediaKeySession::closed(ScriptState* scriptState)
{
    return m_closedPromise->promise(scriptState->world());
}

ScriptPromise MediaKeySession::generateRequest(ScriptState* scriptState, const String& initDataType, ArrayBuffer* initData)
{
    RefPtr<ArrayBuffer> initDataCopy = ArrayBuffer::create(initData->data(), initData->byteLength());
    return generateRequestInternal(scriptState, initDataType, initDataCopy.release());
}

ScriptPromise MediaKeySession::generateRequest(ScriptState* scriptState, const String& initDataType, ArrayBufferView* initData)
{
    RefPtr<ArrayBuffer> initDataCopy = ArrayBuffer::create(initData->baseAddress(), initData->byteLength());
    return generateRequestInternal(scriptState, initDataType, initDataCopy.release());
}

ScriptPromise MediaKeySession::generateRequestInternal(ScriptState* scriptState, const String& initDataType, PassRefPtr<ArrayBuffer> initData)
{
    WTF_LOG(Media, "MediaKeySession(%p)::generateRequest %s", this, initDataType.ascii().data());

    // From https://dvcs.w3.org/hg/html-media/raw-file/default/encrypted-media/encrypted-media.html#dom-generaterequest:
    // The generateRequest(initDataType, initData) method creates a new session
    // for the specified initData. It must run the following steps:

    // 1. If this object's uninitialized value is false, return a promise
    //    rejected with a new DOMException whose name is "InvalidStateError".
    if (!m_isUninitialized) {
        return ScriptPromise::rejectWithDOMException(
            scriptState, DOMException::create(InvalidStateError, "The session is already initialized."));
    }

    // 2. Let this object's uninitialized be false.
    m_isUninitialized = false;

    // 3. If initDataType is an empty string, return a promise rejected with a
    //    new DOMException whose name is "InvalidAccessError".
    if (initDataType.isEmpty()) {
        return ScriptPromise::rejectWithDOMException(
            scriptState, DOMException::create(InvalidAccessError, "The initDataType parameter is empty."));
    }

    // 4. If initData is an empty array, return a promise rejected with a new
    //    DOMException whose name is"InvalidAccessError".
    if (!initData->byteLength()) {
        return ScriptPromise::rejectWithDOMException(
            scriptState, DOMException::create(InvalidAccessError, "The initData parameter is empty."));
    }

    // 5. Let media keys be the MediaKeys object that created this object.
    //    (Use m_mediaKey, which was set in the constructor.)

    // 6. If the content decryption module corresponding to media keys's
    //    keySystem attribute does not support initDataType as an initialization
    //    data type, return a promise rejected with a new DOMException whose
    //    name is "NotSupportedError". String comparison is case-sensitive.
    if (!isKeySystemSupportedWithInitDataType(m_keySystem, initDataType)) {
        return ScriptPromise::rejectWithDOMException(
            scriptState, DOMException::create(NotSupportedError, "The initialization data type '" + initDataType + "' is not supported by the key system."));
    }

    // 7. Let init data be a copy of the contents of the initData parameter.
    //    (Done before calling this method.)

    // 8. Let session type be this object's session type.
    //    (Done in constructor.)

    // 9. Let promise be a new promise.
    NewSessionResult* result = new NewSessionResult(scriptState, this);
    ScriptPromise promise = result->promise();

    // 10. Run the following steps asynchronously (documented in
    //     actionTimerFired())
    m_pendingActions.append(PendingAction::CreatePendingGenerateRequest(result, initDataType, initData));
    ASSERT(!m_actionTimer.isActive());
    m_actionTimer.startOneShot(0, FROM_HERE);

    // 11. Return promise.
    return promise;
}

ScriptPromise MediaKeySession::update(ScriptState* scriptState, ArrayBuffer* response)
{
    RefPtr<ArrayBuffer> responseCopy = ArrayBuffer::create(response->data(), response->byteLength());
    return updateInternal(scriptState, responseCopy.release());
}

ScriptPromise MediaKeySession::update(ScriptState* scriptState, ArrayBufferView* response)
{
    RefPtr<ArrayBuffer> responseCopy = ArrayBuffer::create(response->baseAddress(), response->byteLength());
    return updateInternal(scriptState, responseCopy.release());
}

ScriptPromise MediaKeySession::updateInternal(ScriptState* scriptState, PassRefPtr<ArrayBuffer> response)
{
    WTF_LOG(Media, "MediaKeySession(%p)::update", this);
    ASSERT(!m_isClosed);

    // From <https://dvcs.w3.org/hg/html-media/raw-file/default/encrypted-media/encrypted-media.html#dom-update>:
    // The update(response) method provides messages, including licenses, to the
    // CDM. It must run the following steps:
    //
    // 1. If response is an empty array, return a promise rejected with a new
    //    DOMException whose name is "InvalidAccessError" and that has the
    //    message "The response parameter is empty."
    if (!response->byteLength()) {
        return ScriptPromise::rejectWithDOMException(
            scriptState, DOMException::create(InvalidAccessError, "The response parameter is empty."));
    }

    // 2. Let message be a copy of the contents of the response parameter.
    //    (Copied in the caller.)

    // 3. Let promise be a new promise.
    SimpleContentDecryptionModuleResult* result = new SimpleContentDecryptionModuleResult(scriptState);
    ScriptPromise promise = result->promise();

    // 4. Run the following steps asynchronously (documented in
    //    actionTimerFired())
    m_pendingActions.append(PendingAction::CreatePendingUpdate(result, response));
    if (!m_actionTimer.isActive())
        m_actionTimer.startOneShot(0, FROM_HERE);

    // 5. Return promise.
    return promise;
}

ScriptPromise MediaKeySession::release(ScriptState* scriptState)
{
    WTF_LOG(Media, "MediaKeySession(%p)::release", this);
    SimpleContentDecryptionModuleResult* result = new SimpleContentDecryptionModuleResult(scriptState);
    ScriptPromise promise = result->promise();

    // From <https://dvcs.w3.org/hg/html-media/raw-file/default/encrypted-media/encrypted-media.html#dom-close>:
    // The close() method allows an application to indicate that it no longer
    // needs the session and the CDM should release any resources associated
    // with this object and close it. The returned promise is resolved when the
    // request has been processed, and the closed attribute promise is resolved
    // when the session is closed. It must run the following steps:
    //
    // 1. If the Session Close algorithm has been run on this object, return a
    //    promise fulfilled with undefined.
    if (m_isClosed) {
        result->complete();
        return promise;
    }

    // 2. Let promise be a new promise.
    // (Created earlier so it was available in step 1.)

    // 3. Run the following steps asynchronously (documented in
    //    actionTimerFired()).
    m_pendingActions.append(PendingAction::CreatePendingRelease(result));
    if (!m_actionTimer.isActive())
        m_actionTimer.startOneShot(0, FROM_HERE);

    // 4. Return promise.
    return promise;
}

void MediaKeySession::actionTimerFired(Timer<MediaKeySession>*)
{
    ASSERT(m_pendingActions.size());

    // Resolving promises now run synchronously and may result in additional
    // actions getting added to the queue. As a result, swap the queue to
    // a local copy to avoid problems if this happens.
    HeapDeque<Member<PendingAction> > pendingActions;
    pendingActions.swap(m_pendingActions);

    while (!pendingActions.isEmpty()) {
        PendingAction* action = pendingActions.takeFirst();

        switch (action->type()) {
        case PendingAction::GenerateRequest:
            WTF_LOG(Media, "MediaKeySession(%p)::actionTimerFired: GenerateRequest", this);

            // 10.1 Let request be null.
            // 10.2 Let cdm be the CDM loaded during the initialization of
            //      media keys.
            // 10.3 Use the cdm to execute the following steps:
            // 10.3.1 If the init data is not valid for initDataType, reject
            //        promise with a new DOMException whose name is
            //        "InvalidAccessError".
            // 10.3.2 If the init data is not supported by the cdm, reject
            //        promise with a new DOMException whose name is
            //        "NotSupportedError".
            // 10.3.3 Let request be a request (e.g. a license request)
            //        generated based on the init data, which is interpreted
            //        per initDataType, and session type.
            m_session->initializeNewSession(action->initDataType(), static_cast<unsigned char*>(action->data()->data()), action->data()->byteLength(), m_sessionType, action->result()->result());

            // Remainder of steps executed in finishGenerateRequest(), called
            // when |result| is resolved.
            break;

        case PendingAction::Update:
            WTF_LOG(Media, "MediaKeySession(%p)::actionTimerFired: Update", this);
            // NOTE: Continued from step 4 of MediaKeySession::update().
            // Continue the update call by passing message to the cdm. Once
            // completed, it will resolve/reject the promise.
            m_session->update(static_cast<unsigned char*>(action->data()->data()), action->data()->byteLength(), action->result()->result());
            break;

        case PendingAction::Release:
            WTF_LOG(Media, "MediaKeySession(%p)::actionTimerFired: Release", this);
            // NOTE: Continued from step 3 of MediaKeySession::release().
            // 3.1 Let cdm be the cdm loaded in create().
            // 3.2 Use the cdm to execute the following steps:
            // 3.2.1 Process the close request. Do not remove stored session data.
            // 3.2.2 If the previous step caused the session to be closed, run the
            //       Session Close algorithm on this object.
            // 3.3 Resolve promise with undefined.
            m_session->release(action->result()->result());
            break;
        }
    }
}

void MediaKeySession::finishGenerateRequest()
{
    // 10.4 Set the sessionId attribute to a unique Session ID string.
    //      It may be obtained from cdm.
    ASSERT(!sessionId().isEmpty());

    // 10.5 If any of the preceding steps failed, reject promise with a new
    //      DOMException whose name is the appropriate error name.
    //      (Done by call to completeWithError()).

    // 10.6 Add an entry for the value of the sessionId attribute to
    //      media keys's list of active session IDs.
    // FIXME: Is this required?
    // https://www.w3.org/Bugs/Public/show_bug.cgi?id=26758

    // 10.7 Run the Queue a "message" Event algorithm on the session,
    //      providing request and null.
    //      (Done by the CDM).

    // 10.8 Let this object's callable be true.
    m_isCallable = true;
}

// Queue a task to fire a simple event named keymessage at the new object.
void MediaKeySession::message(const unsigned char* message, size_t messageLength, const WebURL& destinationURL)
{
    WTF_LOG(Media, "MediaKeySession(%p)::message", this);

    // Verify that 'message' not fired before session initialization is complete.
    ASSERT(m_isCallable);

    MediaKeyMessageEventInit init;
    init.bubbles = false;
    init.cancelable = false;
    init.message = ArrayBuffer::create(static_cast<const void*>(message), messageLength);
    init.destinationURL = destinationURL.string();

    RefPtrWillBeRawPtr<MediaKeyMessageEvent> event = MediaKeyMessageEvent::create(EventTypeNames::message, init);
    event->setTarget(this);
    m_asyncEventQueue->enqueueEvent(event.release());
}

void MediaKeySession::ready()
{
    WTF_LOG(Media, "MediaKeySession(%p)::ready", this);

    RefPtrWillBeRawPtr<Event> event = Event::create(EventTypeNames::ready);
    event->setTarget(this);
    m_asyncEventQueue->enqueueEvent(event.release());
}

void MediaKeySession::close()
{
    WTF_LOG(Media, "MediaKeySession(%p)::close", this);

    // Once closed, the session can no longer be the target of events from
    // the CDM so this object can be garbage collected.
    m_isClosed = true;

    // Resolve the closed promise.
    m_closedPromise->resolve(V8UndefinedType());
}

// Queue a task to fire a simple event named keyadded at the MediaKeySession object.
void MediaKeySession::error(MediaKeyErrorCode errorCode, unsigned long systemCode)
{
    WTF_LOG(Media, "MediaKeySession(%p)::error: errorCode=%d, systemCode=%lu", this, errorCode, systemCode);

    MediaKeyError::Code mediaKeyErrorCode = MediaKeyError::MEDIA_KEYERR_UNKNOWN;
    switch (errorCode) {
    case MediaKeyErrorCodeUnknown:
        mediaKeyErrorCode = MediaKeyError::MEDIA_KEYERR_UNKNOWN;
        break;
    case MediaKeyErrorCodeClient:
        mediaKeyErrorCode = MediaKeyError::MEDIA_KEYERR_CLIENT;
        break;
    }

    // 1. Create a new MediaKeyError object with the following attributes:
    //    code = the appropriate MediaKeyError code
    //    systemCode = a Key System-specific value, if provided, and 0 otherwise
    // 2. Set the MediaKeySession object's error attribute to the error object created in the previous step.
    m_error = MediaKeyError::create(mediaKeyErrorCode, systemCode);

    // 3. queue a task to fire a simple event named keyerror at the MediaKeySession object.
    RefPtrWillBeRawPtr<Event> event = Event::create(EventTypeNames::error);
    event->setTarget(this);
    m_asyncEventQueue->enqueueEvent(event.release());
}

void MediaKeySession::error(WebContentDecryptionModuleException exception, unsigned long systemCode, const WebString& errorMessage)
{
    WTF_LOG(Media, "MediaKeySession::error: exception=%d, systemCode=%lu", exception, systemCode);

    // FIXME: EME-WD MediaKeyError now derives from DOMException. Figure out how
    // to implement this without breaking prefixed EME, which has a totally
    // different definition. The spec may also change to be just a DOMException.
    // For now, simply generate an existing MediaKeyError.
    MediaKeyErrorCode errorCode;
    switch (exception) {
    case WebContentDecryptionModuleExceptionClientError:
        errorCode = MediaKeyErrorCodeClient;
        break;
    default:
        // All other exceptions get converted into Unknown.
        errorCode = MediaKeyErrorCodeUnknown;
        break;
    }
    error(errorCode, systemCode);
}

void MediaKeySession::expirationChanged(double updatedExpiryTimeInMS)
{
    // FIXME: Implement expiration attribute.
}

const AtomicString& MediaKeySession::interfaceName() const
{
    return EventTargetNames::MediaKeySession;
}

ExecutionContext* MediaKeySession::executionContext() const
{
    return ActiveDOMObject::executionContext();
}

bool MediaKeySession::hasPendingActivity() const
{
    // Remain around if there are pending events or MediaKeys is still around
    // and we're not closed.
    WTF_LOG(Media, "MediaKeySession(%p)::hasPendingActivity %s%s%s%s", this,
        ActiveDOMObject::hasPendingActivity() ? " ActiveDOMObject::hasPendingActivity()" : "",
        !m_pendingActions.isEmpty() ? " !m_pendingActions.isEmpty()" : "",
        m_asyncEventQueue->hasPendingEvents() ? " m_asyncEventQueue->hasPendingEvents()" : "",
        (m_mediaKeys && !m_isClosed) ? " m_mediaKeys && !m_isClosed" : "");

    return ActiveDOMObject::hasPendingActivity()
        || !m_pendingActions.isEmpty()
        || m_asyncEventQueue->hasPendingEvents()
        || (m_mediaKeys && !m_isClosed);
}

void MediaKeySession::stop()
{
    // Stop the CDM from firing any more events for this session.
    m_session.clear();
    m_isClosed = true;

    if (m_actionTimer.isActive())
        m_actionTimer.stop();
    m_pendingActions.clear();
    m_asyncEventQueue->close();
}

void MediaKeySession::trace(Visitor* visitor)
{
    visitor->trace(m_error);
    visitor->trace(m_asyncEventQueue);
    visitor->trace(m_pendingActions);
    visitor->trace(m_mediaKeys);
    visitor->trace(m_closedPromise);
    EventTargetWithInlineData::trace(visitor);
}

} // namespace blink
