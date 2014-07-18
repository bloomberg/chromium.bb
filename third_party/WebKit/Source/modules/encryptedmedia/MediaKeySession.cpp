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

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/events/Event.h"
#include "core/events/GenericEventQueue.h"
#include "core/html/MediaKeyError.h"
#include "modules/encryptedmedia/MediaKeyMessageEvent.h"
#include "modules/encryptedmedia/MediaKeys.h"
#include "modules/encryptedmedia/SimpleContentDecryptionModuleResult.h"
#include "platform/ContentDecryptionModuleResult.h"
#include "platform/Logging.h"
#include "platform/Timer.h"
#include "public/platform/WebContentDecryptionModule.h"
#include "public/platform/WebContentDecryptionModuleException.h"
#include "public/platform/WebContentDecryptionModuleSession.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "wtf/Uint8Array.h"

namespace blink {

// A class holding a pending action.
class MediaKeySession::PendingAction : public GarbageCollectedFinalized<MediaKeySession::PendingAction> {
public:
    enum Type {
        Update,
        Release
    };

    Type type() const { return m_type; }
    const Persistent<ContentDecryptionModuleResult> result() const { return m_result; }
    // |data| is only valid for Update types.
    const RefPtr<Uint8Array> data() const { return m_data; }

    static PendingAction* CreatePendingUpdate(ContentDecryptionModuleResult* result, PassRefPtr<Uint8Array> data)
    {
        ASSERT(result);
        ASSERT(data);
        return new PendingAction(Update, result, data);
    }

    static PendingAction* CreatePendingRelease(ContentDecryptionModuleResult* result)
    {
        ASSERT(result);
        return new PendingAction(Release, result, PassRefPtr<Uint8Array>());
    }

    ~PendingAction()
    {
    }

    void trace(Visitor* visitor)
    {
        visitor->trace(m_result);
    }

private:
    PendingAction(Type type, ContentDecryptionModuleResult* result, PassRefPtr<Uint8Array> data)
        : m_type(type)
        , m_result(result)
        , m_data(data)
    {
    }

    const Type m_type;
    const Member<ContentDecryptionModuleResult> m_result;
    const RefPtr<Uint8Array> m_data;
};

// This class allows a MediaKeySession object to be created asynchronously.
class MediaKeySessionInitializer : public ScriptPromiseResolver {
    WTF_MAKE_NONCOPYABLE(MediaKeySessionInitializer);

public:
    static ScriptPromise create(ScriptState*, MediaKeys*, const String& initDataType, PassRefPtr<Uint8Array> initData, const String& sessionType);
    virtual ~MediaKeySessionInitializer();

    void completeWithSession(blink::WebContentDecryptionModuleResult::SessionStatus);
    void completeWithDOMException(ExceptionCode, const String& errorMessage);

private:
    MediaKeySessionInitializer(ScriptState*, MediaKeys*, const String& initDataType, PassRefPtr<Uint8Array> initData, const String& sessionType);
    void timerFired(Timer<MediaKeySessionInitializer>*);

    Persistent<MediaKeys> m_mediaKeys;
    OwnPtr<blink::WebContentDecryptionModuleSession> m_cdmSession;

    // The next 3 values are simply the initialization data saved so that the
    // asynchronous creation has the data needed.
    String m_initDataType;
    RefPtr<Uint8Array> m_initData;
    String m_sessionType;

    Timer<MediaKeySessionInitializer> m_timer;
};

// Represents the result used when a new WebContentDecryptionModuleSession
// object has been created. Needed as MediaKeySessionInitializer can't be both
// a ScriptPromiseResolver and ContentDecryptionModuleResult at the same time.
class NewMediaKeySessionResult FINAL : public ContentDecryptionModuleResult {
public:
    NewMediaKeySessionResult(MediaKeySessionInitializer* initializer)
        : m_initializer(initializer)
    {
    }

    // ContentDecryptionModuleResult implementation.
    virtual void complete() OVERRIDE
    {
        ASSERT_NOT_REACHED();
        m_initializer->completeWithDOMException(InvalidStateError, "Unexpected completion.");
    }

    virtual void completeWithSession(blink::WebContentDecryptionModuleResult::SessionStatus status) OVERRIDE
    {
        m_initializer->completeWithSession(status);
    }

    virtual void completeWithError(blink::WebContentDecryptionModuleException code, unsigned long systemCode, const blink::WebString& message) OVERRIDE
    {
        m_initializer->completeWithDOMException(WebCdmExceptionToExceptionCode(code), message);
    }

private:
    MediaKeySessionInitializer* m_initializer;
};

ScriptPromise MediaKeySessionInitializer::create(ScriptState* scriptState, MediaKeys* mediaKeys, const String& initDataType, PassRefPtr<Uint8Array> initData, const String& sessionType)
{
    RefPtr<MediaKeySessionInitializer> initializer = adoptRef(new MediaKeySessionInitializer(scriptState, mediaKeys, initDataType, initData, sessionType));
    initializer->suspendIfNeeded();
    initializer->keepAliveWhilePending();
    return initializer->promise();
}

MediaKeySessionInitializer::MediaKeySessionInitializer(ScriptState* scriptState, MediaKeys* mediaKeys, const String& initDataType, PassRefPtr<Uint8Array> initData, const String& sessionType)
    : ScriptPromiseResolver(scriptState)
    , m_mediaKeys(mediaKeys)
    , m_initDataType(initDataType)
    , m_initData(initData)
    , m_sessionType(sessionType)
    , m_timer(this, &MediaKeySessionInitializer::timerFired)
{
    WTF_LOG(Media, "MediaKeySessionInitializer::MediaKeySessionInitializer");

    // Start the timer so that MediaKeySession can be created asynchronously.
    m_timer.startOneShot(0, FROM_HERE);
}

MediaKeySessionInitializer::~MediaKeySessionInitializer()
{
    WTF_LOG(Media, "MediaKeySessionInitializer::~MediaKeySessionInitializer");
}

void MediaKeySessionInitializer::timerFired(Timer<MediaKeySessionInitializer>*)
{
    WTF_LOG(Media, "MediaKeySessionInitializer::timerFired");

    // Continue MediaKeys::createSession() at step 7.
    // 7.1 Let request be null. (Request provided by cdm in message event).
    // 7.2 Let default URL be null. (Also provided by cdm in message event).

    // 7.3 Let cdm be the cdm loaded in create().
    blink::WebContentDecryptionModule* cdm = m_mediaKeys->contentDecryptionModule();

    // 7.4 Use the cdm to execute the following steps:
    // 7.4.1 If the init data is not valid for initDataType, reject promise
    //       with a new DOMException whose name is "InvalidAccessError".
    // 7.4.2 If the init data is not supported by the cdm, reject promise with
    //       a new DOMException whose name is "NotSupportedError".
    // 7.4.3 Let request be a request (e.g. a license request) generated based
    //       on the init data, which is interpreteted per initDataType, and
    //       sessionType. If sessionType is "temporary", the request is for a
    //       temporary non-persisted license. If sessionType is "persistent",
    //       the request is for a persistable license.
    // 7.4.4 If the init data indicates a default URL, let default URL be
    //       that URL. The URL may be validated and/or normalized.
    m_cdmSession = adoptPtr(cdm->createSession());
    NewMediaKeySessionResult* result = new NewMediaKeySessionResult(this);
    m_cdmSession->initializeNewSession(m_initDataType, m_initData->data(), m_initData->length(), m_sessionType, result->result());

    WTF_LOG(Media, "MediaKeySessionInitializer::timerFired done");
    // Note: As soon as the promise is resolved (or rejected), the
    // ScriptPromiseResolver object (|this|) is freed. So if
    // initializeNewSession() is synchronous, access to any members will crash.
}

void MediaKeySessionInitializer::completeWithSession(blink::WebContentDecryptionModuleResult::SessionStatus status)
{
    WTF_LOG(Media, "MediaKeySessionInitializer::completeWithSession");

    switch (status) {
    case blink::WebContentDecryptionModuleResult::NewSession: {
        // Resume MediaKeys::createSession().
        // 7.5 Let the session ID be a unique Session ID string. It may be
        //     obtained from cdm (it is).
        // 7.6 Let session be a new MediaKeySession object, and initialize it.
        //     (Object was created previously, complete the steps for 7.6).
        RefPtrWillBeRawPtr<MediaKeySession> session = adoptRefCountedGarbageCollectedWillBeNoop(new MediaKeySession(executionContext(), m_mediaKeys, m_cdmSession.release()));
        session->suspendIfNeeded();

        // 7.7 If any of the preceding steps failed, reject promise with a
        //     new DOMException whose name is the appropriate error name
        //     and that has an appropriate message.
        //     (Implemented by CDM/Chromium calling completeWithError()).

        // 7.8 Add an entry for the value of the sessionId attribute to the
        //     list of active session IDs for this object.
        //     (Implemented in SessionIdAdapter).

        // 7.9 Run the Queue a "message" Event algorithm on the session,
        //     providing request and default URL.
        //     (Done by the CDM).

        // 7.10 Resolve promise with session.
        resolve(session.release());
        WTF_LOG(Media, "MediaKeySessionInitializer::completeWithSession done w/session");
        return;
    }

    case blink::WebContentDecryptionModuleResult::SessionNotFound:
        // Step 4.7.1 of MediaKeys::loadSession(): If there is no data
        // stored for the sessionId in the origin, resolve promise with
        // undefined.
        resolve(V8UndefinedType());
        WTF_LOG(Media, "MediaKeySessionInitializer::completeWithSession done w/undefined");
        return;

    case blink::WebContentDecryptionModuleResult::SessionAlreadyExists:
        // If a session already exists, resolve the promise with null.
        resolve(V8NullType());
        WTF_LOG(Media, "MediaKeySessionInitializer::completeWithSession done w/null");
        return;
    }
    ASSERT_NOT_REACHED();
}

void MediaKeySessionInitializer::completeWithDOMException(ExceptionCode code, const String& errorMessage)
{
    WTF_LOG(Media, "MediaKeySessionInitializer::completeWithDOMException");
    reject(DOMException::create(code, errorMessage));
}

ScriptPromise MediaKeySession::create(ScriptState* scriptState, MediaKeys* mediaKeys, const String& initDataType, PassRefPtr<Uint8Array> initData, const String& sessionType)
{
    // Since creation is done asynchronously, use MediaKeySessionInitializer
    // to do it.
    return MediaKeySessionInitializer::create(scriptState, mediaKeys, initDataType, initData, sessionType);
}

MediaKeySession::MediaKeySession(ExecutionContext* context, MediaKeys* keys, PassOwnPtr<blink::WebContentDecryptionModuleSession> cdmSession)
    : ActiveDOMObject(context)
    , m_keySystem(keys->keySystem())
    , m_asyncEventQueue(GenericEventQueue::create(this))
    , m_session(cdmSession)
    , m_keys(keys)
    , m_isClosed(false)
    , m_actionTimer(this, &MediaKeySession::actionTimerFired)
{
    WTF_LOG(Media, "MediaKeySession(%p)::MediaKeySession", this);
    ScriptWrappable::init(this);
    m_session->setClientInterface(this);

    // Resume MediaKeys::createSession() at step 7.6.
    // 7.6.1 Set the error attribute to null.
    ASSERT(!m_error);

    // 7.6.2 Set the sessionId attribute to session ID.
    ASSERT(!sessionId().isEmpty());

    // 7.6.3 Let expiration be NaN.
    // 7.6.4 Let closed be a new promise.
    // 7.6.5 Let the session type be sessionType.
    // FIXME: Implement the previous 3 values.
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

ScriptPromise MediaKeySession::update(ScriptState* scriptState, Uint8Array* response)
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
    if (!response->length()) {
        return ScriptPromise::rejectWithDOMException(
            scriptState, DOMException::create(InvalidAccessError, "The response parameter is empty."));
    }

    // 2. Let message be a copy of the contents of the response parameter.
    RefPtr<Uint8Array> responseCopy = Uint8Array::create(response->data(), response->length());

    // 3. Let promise be a new promise.
    SimpleContentDecryptionModuleResult* result = new SimpleContentDecryptionModuleResult(scriptState);
    ScriptPromise promise = result->promise();

    // 4. Run the following steps asynchronously (documented in
    //    actionTimerFired())
    m_pendingActions.append(PendingAction::CreatePendingUpdate(result, responseCopy.release()));
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
        case PendingAction::Update:
            WTF_LOG(Media, "MediaKeySession(%p)::actionTimerFired: Update", this);
            // NOTE: Continued from step 4 of MediaKeySession::update().
            // Continue the update call by passing message to the cdm. Once
            // completed, it will resolve/reject the promise.
            m_session->update(action->data()->data(), action->data()->length(), action->result()->result());
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

// Queue a task to fire a simple event named keymessage at the new object
void MediaKeySession::message(const unsigned char* message, size_t messageLength, const blink::WebURL& destinationURL)
{
    WTF_LOG(Media, "MediaKeySession(%p)::message", this);

    MediaKeyMessageEventInit init;
    init.bubbles = false;
    init.cancelable = false;
    init.message = Uint8Array::create(message, messageLength);
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

    // FIXME: Implement closed() attribute.
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

void MediaKeySession::error(blink::WebContentDecryptionModuleException exception, unsigned long systemCode, const blink::WebString& errorMessage)
{
    WTF_LOG(Media, "MediaKeySession::error: exception=%d, systemCode=%lu", exception, systemCode);

    // FIXME: EME-WD MediaKeyError now derives from DOMException. Figure out how
    // to implement this without breaking prefixed EME, which has a totally
    // different definition. The spec may also change to be just a DOMException.
    // For now, simply generate an existing MediaKeyError.
    MediaKeyErrorCode errorCode;
    switch (exception) {
    case blink::WebContentDecryptionModuleExceptionClientError:
        errorCode = MediaKeyErrorCodeClient;
        break;
    default:
        // All other exceptions get converted into Unknown.
        errorCode = MediaKeyErrorCodeUnknown;
        break;
    }
    error(errorCode, systemCode);
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
        (m_keys && !m_isClosed) ? " m_keys && !m_isClosed" : "");

    return ActiveDOMObject::hasPendingActivity()
        || !m_pendingActions.isEmpty()
        || m_asyncEventQueue->hasPendingEvents()
        || (m_keys && !m_isClosed);
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
    visitor->trace(m_keys);
    EventTargetWithInlineData::trace(visitor);
}

}
