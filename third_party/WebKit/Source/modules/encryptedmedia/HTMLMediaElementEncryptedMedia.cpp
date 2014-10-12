// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/encryptedmedia/HTMLMediaElementEncryptedMedia.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/MediaKeyError.h"
#include "core/html/MediaKeyEvent.h"
#include "modules/encryptedmedia/MediaKeyNeededEvent.h"
#include "modules/encryptedmedia/MediaKeys.h"
#include "modules/encryptedmedia/SimpleContentDecryptionModuleResult.h"
#include "platform/ContentDecryptionModuleResult.h"
#include "platform/Logging.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "wtf/Functional.h"
#include "wtf/Uint8Array.h"

namespace blink {

static void throwExceptionIfMediaKeyExceptionOccurred(const String& keySystem, const String& sessionId, WebMediaPlayer::MediaKeyException exception, ExceptionState& exceptionState)
{
    switch (exception) {
    case WebMediaPlayer::MediaKeyExceptionNoError:
        return;
    case WebMediaPlayer::MediaKeyExceptionInvalidPlayerState:
        exceptionState.throwDOMException(InvalidStateError, "The player is in an invalid state.");
        return;
    case WebMediaPlayer::MediaKeyExceptionKeySystemNotSupported:
        exceptionState.throwDOMException(NotSupportedError, "The key system provided ('" + keySystem +"') is not supported.");
        return;
    case WebMediaPlayer::MediaKeyExceptionInvalidAccess:
        exceptionState.throwDOMException(InvalidAccessError, "The session ID provided ('" + sessionId + "') is invalid.");
        return;
    }

    ASSERT_NOT_REACHED();
    return;
}

// This class allows MediaKeys to be set asynchronously.
class SetMediaKeysHandler : public ScriptPromiseResolver {
    WTF_MAKE_NONCOPYABLE(SetMediaKeysHandler);

public:
    static ScriptPromise create(ScriptState*, HTMLMediaElement&, MediaKeys*);
    virtual ~SetMediaKeysHandler();

private:
    SetMediaKeysHandler(ScriptState*, HTMLMediaElement&, MediaKeys*);
    void timerFired(Timer<SetMediaKeysHandler>*);

    void clearExistingMediaKeys();
    void setNewMediaKeys();
    void finish();

    void reportSetFailed(ExceptionCode, const String& errorMessage);

    // Keep media element alive until promise is fulfilled
    RefPtrWillBePersistent<HTMLMediaElement> m_element;
    Persistent<MediaKeys> m_newMediaKeys;
    Timer<SetMediaKeysHandler> m_timer;
};

typedef Function<void()> SuccessCallback;
typedef Function<void(ExceptionCode, const String&)> FailureCallback;

// Represents the result used when setContentDecryptionModule() is called.
// Calls |success| if result is resolved, |failure| is result is rejected.
class SetContentDecryptionModuleResult final : public ContentDecryptionModuleResult {
public:
    SetContentDecryptionModuleResult(SuccessCallback success, FailureCallback failure)
        : m_successCallback(success)
        , m_failureCallback(failure)
    {
    }

    // ContentDecryptionModuleResult implementation.
    virtual void complete() override
    {
        m_successCallback();
    }

    virtual void completeWithSession(blink::WebContentDecryptionModuleResult::SessionStatus status) override
    {
        ASSERT_NOT_REACHED();
        m_failureCallback(InvalidStateError, "Unexpected completion.");
    }

    virtual void completeWithError(blink::WebContentDecryptionModuleException code, unsigned long systemCode, const blink::WebString& message) override
    {
        m_failureCallback(WebCdmExceptionToExceptionCode(code), message);
    }

private:
    SuccessCallback m_successCallback;
    FailureCallback m_failureCallback;
};

ScriptPromise SetMediaKeysHandler::create(ScriptState* scriptState, HTMLMediaElement& element, MediaKeys* mediaKeys)
{
    RefPtr<SetMediaKeysHandler> handler = adoptRef(new SetMediaKeysHandler(scriptState, element, mediaKeys));
    handler->suspendIfNeeded();
    handler->keepAliveWhilePending();
    return handler->promise();
}

SetMediaKeysHandler::SetMediaKeysHandler(ScriptState* scriptState, HTMLMediaElement& element, MediaKeys* mediaKeys)
    : ScriptPromiseResolver(scriptState)
    , m_element(element)
    , m_newMediaKeys(mediaKeys)
    , m_timer(this, &SetMediaKeysHandler::timerFired)
{
    WTF_LOG(Media, "SetMediaKeysHandler::SetMediaKeysHandler");

    // 3. Run the remaining steps asynchronously.
    m_timer.startOneShot(0, FROM_HERE);
}

SetMediaKeysHandler::~SetMediaKeysHandler()
{
}

void SetMediaKeysHandler::timerFired(Timer<SetMediaKeysHandler>*)
{
    clearExistingMediaKeys();
}

void SetMediaKeysHandler::clearExistingMediaKeys()
{
    WTF_LOG(Media, "SetMediaKeysHandler::clearExistingMediaKeys");
    HTMLMediaElementEncryptedMedia& thisElement = HTMLMediaElementEncryptedMedia::from(*m_element);

    // 3.1 If mediaKeys is not null, it is already in use by another media
    //     element, and the user agent is unable to use it with this element,
    //     reject promise with a new DOMException whose name is
    //     "QuotaExceededError".
    // FIXME: Need to check whether mediaKeys is already in use by another
    //        media element.
    // 3.2 If the mediaKeys attribute is not null, run the following steps:
    if (thisElement.m_mediaKeys) {
        // 3.2.1 If the user agent or CDM do not support removing the
        //       association, return a promise rejected with a new DOMException
        //       whose name is "NotSupportedError".
        //       (supported by blink).
        // 3.2.2 If the association cannot currently be removed (i.e. during
        //       playback), return a promise rejected with a new DOMException
        //       whose name is "InvalidStateError".
        if (m_element->webMediaPlayer()) {
            reject(DOMException::create(InvalidStateError, "The existing MediaKeys object cannot be removed while a media resource is loaded."));
            return;
        }

        // (next 2 steps not required as there is no player connected).
        // 3.2.3 Stop using the CDM instance represented by the mediaKeys
        //       attribute to decrypt media data and remove the association
        //       with the media element.
        // 3.2.4 If the preceding step failed, reject promise with a new
        //       DOMException whose name is the appropriate error name and
        //       that has an appropriate message.
    }

    // MediaKeys not currently set or no player connected, so continue on.
    setNewMediaKeys();
}

void SetMediaKeysHandler::setNewMediaKeys()
{
    WTF_LOG(Media, "SetMediaKeysHandler::setNewMediaKeys");

    // 3.3 If mediaKeys is not null, run the following steps:
    if (m_newMediaKeys) {
        // 3.3.1 Associate the CDM instance represented by mediaKeys with the
        //       media element for decrypting media data.
        // 3.3.2 If the preceding step failed, run the following steps:
        //       (done in reportSetFailed()).
        // 3.3.3 Run the Attempt to Resume Playback If Necessary algorithm on
        //       the media element. The user agent may choose to skip this
        //       step if it knows resuming will fail (i.e. mediaKeys has no
        //       sessions).
        //       (Handled in Chromium).
        if (m_element->webMediaPlayer()) {
            SuccessCallback successCallback = bind(&SetMediaKeysHandler::finish, this);
            FailureCallback failureCallback = bind<ExceptionCode, const String&>(&SetMediaKeysHandler::reportSetFailed, this);
            ContentDecryptionModuleResult* result = new SetContentDecryptionModuleResult(successCallback, failureCallback);
            m_element->webMediaPlayer()->setContentDecryptionModule(m_newMediaKeys->contentDecryptionModule(), result->result());

            // Don't do anything more until |result| is resolved (or rejected).
            return;
        }
    }

    // MediaKeys doesn't need to be set on the player, so continue on.
    finish();
}

void SetMediaKeysHandler::finish()
{
    WTF_LOG(Media, "SetMediaKeysHandler::finish");
    HTMLMediaElementEncryptedMedia& thisElement = HTMLMediaElementEncryptedMedia::from(*m_element);

    // 3.4 Set the mediaKeys attribute to mediaKeys.
    thisElement.m_mediaKeys = m_newMediaKeys;

    // 3.5 Resolve promise with undefined.
    resolve();
}

void SetMediaKeysHandler::reportSetFailed(ExceptionCode code, const String& errorMessage)
{
    WTF_LOG(Media, "SetMediaKeysHandler::reportSetFailed");
    HTMLMediaElementEncryptedMedia& thisElement = HTMLMediaElementEncryptedMedia::from(*m_element);

    // 3.3.2 If the preceding step failed, run the following steps:
    // 3.3.2.1 Set the mediaKeys attribute to null.
    thisElement.m_mediaKeys.clear();

    // 3.3.2.2 Reject promise with a new DOMException whose name is the
    //         appropriate error name and that has an appropriate message.
    reject(DOMException::create(code, errorMessage));
}

HTMLMediaElementEncryptedMedia::HTMLMediaElementEncryptedMedia()
    : m_emeMode(EmeModeNotSelected)
{
}

DEFINE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(HTMLMediaElementEncryptedMedia)

const char* HTMLMediaElementEncryptedMedia::supplementName()
{
    return "HTMLMediaElementEncryptedMedia";
}

HTMLMediaElementEncryptedMedia& HTMLMediaElementEncryptedMedia::from(HTMLMediaElement& element)
{
    HTMLMediaElementEncryptedMedia* supplement = static_cast<HTMLMediaElementEncryptedMedia*>(WillBeHeapSupplement<HTMLMediaElement>::from(element, supplementName()));
    if (!supplement) {
        supplement = new HTMLMediaElementEncryptedMedia();
        provideTo(element, supplementName(), adoptPtrWillBeNoop(supplement));
    }
    return *supplement;
}

bool HTMLMediaElementEncryptedMedia::setEmeMode(EmeMode emeMode)
{
    if (m_emeMode != EmeModeNotSelected && m_emeMode != emeMode)
        return false;

    m_emeMode = emeMode;
    return true;
}

WebContentDecryptionModule* HTMLMediaElementEncryptedMedia::contentDecryptionModule()
{
    return m_mediaKeys ? m_mediaKeys->contentDecryptionModule() : 0;
}

MediaKeys* HTMLMediaElementEncryptedMedia::mediaKeys(HTMLMediaElement& element)
{
    HTMLMediaElementEncryptedMedia& thisElement = HTMLMediaElementEncryptedMedia::from(element);
    return thisElement.m_mediaKeys.get();
}

ScriptPromise HTMLMediaElementEncryptedMedia::setMediaKeys(ScriptState* scriptState, HTMLMediaElement& element, MediaKeys* mediaKeys)
{
    HTMLMediaElementEncryptedMedia& thisElement = HTMLMediaElementEncryptedMedia::from(element);
    WTF_LOG(Media, "HTMLMediaElementEncryptedMedia::setMediaKeys current(%p), new(%p)", thisElement.m_mediaKeys.get(), mediaKeys);

    if (!thisElement.setEmeMode(EmeModeUnprefixed))
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(InvalidStateError, "Mixed use of EME prefixed and unprefixed API not allowed."));

    // 1. If mediaKeys and the mediaKeys attribute are the same object, return
    //    a promise resolved with undefined.
    if (thisElement.m_mediaKeys == mediaKeys)
        return ScriptPromise::cast(scriptState, V8ValueTraits<V8UndefinedType>::toV8Value(V8UndefinedType(), scriptState->context()->Global(), scriptState->isolate()));

    // 2. Let promise be a new promise. Remaining steps done in handler.
    return SetMediaKeysHandler::create(scriptState, element, mediaKeys);
}

// Create a MediaKeyNeededEvent for WD EME.
static PassRefPtrWillBeRawPtr<Event> createNeedKeyEvent(const String& contentType, const unsigned char* initData, unsigned initDataLength)
{
    MediaKeyNeededEventInit initializer;
    initializer.contentType = contentType;
    initializer.initData = Uint8Array::create(initData, initDataLength);
    initializer.bubbles = false;
    initializer.cancelable = false;

    return MediaKeyNeededEvent::create(EventTypeNames::needkey, initializer);
}

// Create a 'needkey' MediaKeyEvent for v0.1b EME.
static PassRefPtrWillBeRawPtr<Event> createWebkitNeedKeyEvent(const String& contentType, const unsigned char* initData, unsigned initDataLength)
{
    MediaKeyEventInit webkitInitializer;
    webkitInitializer.keySystem = String();
    webkitInitializer.sessionId = String();
    webkitInitializer.initData = Uint8Array::create(initData, initDataLength);
    webkitInitializer.bubbles = false;
    webkitInitializer.cancelable = false;

    return MediaKeyEvent::create(EventTypeNames::webkitneedkey, webkitInitializer);
}

void HTMLMediaElementEncryptedMedia::webkitGenerateKeyRequest(HTMLMediaElement& element, const String& keySystem, PassRefPtr<Uint8Array> initData, ExceptionState& exceptionState)
{
    HTMLMediaElementEncryptedMedia::from(element).generateKeyRequest(element.webMediaPlayer(), keySystem, initData, exceptionState);
}

void HTMLMediaElementEncryptedMedia::generateKeyRequest(WebMediaPlayer* webMediaPlayer, const String& keySystem, PassRefPtr<Uint8Array> initData, ExceptionState& exceptionState)
{
    WTF_LOG(Media, "HTMLMediaElementEncryptedMedia::webkitGenerateKeyRequest");

    if (!setEmeMode(EmeModePrefixed)) {
        exceptionState.throwDOMException(InvalidStateError, "Mixed use of EME prefixed and unprefixed API not allowed.");
        return;
    }

    if (keySystem.isEmpty()) {
        exceptionState.throwDOMException(SyntaxError, "The key system provided is empty.");
        return;
    }

    if (!webMediaPlayer) {
        exceptionState.throwDOMException(InvalidStateError, "No media has been loaded.");
        return;
    }

    const unsigned char* initDataPointer = 0;
    unsigned initDataLength = 0;
    if (initData) {
        initDataPointer = initData->data();
        initDataLength = initData->length();
    }

    WebMediaPlayer::MediaKeyException result = webMediaPlayer->generateKeyRequest(keySystem, initDataPointer, initDataLength);
    throwExceptionIfMediaKeyExceptionOccurred(keySystem, String(), result, exceptionState);
}

void HTMLMediaElementEncryptedMedia::webkitGenerateKeyRequest(HTMLMediaElement& mediaElement, const String& keySystem, ExceptionState& exceptionState)
{
    webkitGenerateKeyRequest(mediaElement, keySystem, Uint8Array::create(0), exceptionState);
}

void HTMLMediaElementEncryptedMedia::webkitAddKey(HTMLMediaElement& element, const String& keySystem, PassRefPtr<Uint8Array> key, PassRefPtr<Uint8Array> initData, const String& sessionId, ExceptionState& exceptionState)
{
    HTMLMediaElementEncryptedMedia::from(element).addKey(element.webMediaPlayer(), keySystem, key, initData, sessionId, exceptionState);
}

void HTMLMediaElementEncryptedMedia::addKey(WebMediaPlayer* webMediaPlayer, const String& keySystem, PassRefPtr<Uint8Array> key, PassRefPtr<Uint8Array> initData, const String& sessionId, ExceptionState& exceptionState)
{
    WTF_LOG(Media, "HTMLMediaElementEncryptedMedia::webkitAddKey");

    if (!setEmeMode(EmeModePrefixed)) {
        exceptionState.throwDOMException(InvalidStateError, "Mixed use of EME prefixed and unprefixed API not allowed.");
        return;
    }

    if (keySystem.isEmpty()) {
        exceptionState.throwDOMException(SyntaxError, "The key system provided is empty.");
        return;
    }

    if (!key) {
        exceptionState.throwDOMException(SyntaxError, "The key provided is invalid.");
        return;
    }

    if (!key->length()) {
        exceptionState.throwDOMException(TypeMismatchError, "The key provided is invalid.");
        return;
    }

    if (!webMediaPlayer) {
        exceptionState.throwDOMException(InvalidStateError, "No media has been loaded.");
        return;
    }

    const unsigned char* initDataPointer = 0;
    unsigned initDataLength = 0;
    if (initData) {
        initDataPointer = initData->data();
        initDataLength = initData->length();
    }

    WebMediaPlayer::MediaKeyException result = webMediaPlayer->addKey(keySystem, key->data(), key->length(), initDataPointer, initDataLength, sessionId);
    throwExceptionIfMediaKeyExceptionOccurred(keySystem, sessionId, result, exceptionState);
}

void HTMLMediaElementEncryptedMedia::webkitAddKey(HTMLMediaElement& mediaElement, const String& keySystem, PassRefPtr<Uint8Array> key, ExceptionState& exceptionState)
{
    webkitAddKey(mediaElement, keySystem, key, Uint8Array::create(0), String(), exceptionState);
}

void HTMLMediaElementEncryptedMedia::webkitCancelKeyRequest(HTMLMediaElement& element, const String& keySystem, const String& sessionId, ExceptionState& exceptionState)
{
    HTMLMediaElementEncryptedMedia::from(element).cancelKeyRequest(element.webMediaPlayer(), keySystem, sessionId, exceptionState);
}

void HTMLMediaElementEncryptedMedia::cancelKeyRequest(WebMediaPlayer* webMediaPlayer, const String& keySystem, const String& sessionId, ExceptionState& exceptionState)
{
    WTF_LOG(Media, "HTMLMediaElementEncryptedMedia::webkitCancelKeyRequest");

    if (!setEmeMode(EmeModePrefixed)) {
        exceptionState.throwDOMException(InvalidStateError, "Mixed use of EME prefixed and unprefixed API not allowed.");
        return;
    }

    if (keySystem.isEmpty()) {
        exceptionState.throwDOMException(SyntaxError, "The key system provided is empty.");
        return;
    }

    if (!webMediaPlayer) {
        exceptionState.throwDOMException(InvalidStateError, "No media has been loaded.");
        return;
    }

    WebMediaPlayer::MediaKeyException result = webMediaPlayer->cancelKeyRequest(keySystem, sessionId);
    throwExceptionIfMediaKeyExceptionOccurred(keySystem, sessionId, result, exceptionState);
}

void HTMLMediaElementEncryptedMedia::keyAdded(HTMLMediaElement& element, const String& keySystem, const String& sessionId)
{
    WTF_LOG(Media, "HTMLMediaElementEncryptedMedia::mediaPlayerKeyAdded");

    MediaKeyEventInit initializer;
    initializer.keySystem = keySystem;
    initializer.sessionId = sessionId;
    initializer.bubbles = false;
    initializer.cancelable = false;

    RefPtrWillBeRawPtr<Event> event = MediaKeyEvent::create(EventTypeNames::webkitkeyadded, initializer);
    event->setTarget(&element);
    element.scheduleEvent(event.release());
}

void HTMLMediaElementEncryptedMedia::keyError(HTMLMediaElement& element, const String& keySystem, const String& sessionId, WebMediaPlayerClient::MediaKeyErrorCode errorCode, unsigned short systemCode)
{
    WTF_LOG(Media, "HTMLMediaElementEncryptedMedia::mediaPlayerKeyError: sessionID=%s, errorCode=%d, systemCode=%d", sessionId.utf8().data(), errorCode, systemCode);

    MediaKeyError::Code mediaKeyErrorCode = MediaKeyError::MEDIA_KEYERR_UNKNOWN;
    switch (errorCode) {
    case WebMediaPlayerClient::MediaKeyErrorCodeUnknown:
        mediaKeyErrorCode = MediaKeyError::MEDIA_KEYERR_UNKNOWN;
        break;
    case WebMediaPlayerClient::MediaKeyErrorCodeClient:
        mediaKeyErrorCode = MediaKeyError::MEDIA_KEYERR_CLIENT;
        break;
    case WebMediaPlayerClient::MediaKeyErrorCodeService:
        mediaKeyErrorCode = MediaKeyError::MEDIA_KEYERR_SERVICE;
        break;
    case WebMediaPlayerClient::MediaKeyErrorCodeOutput:
        mediaKeyErrorCode = MediaKeyError::MEDIA_KEYERR_OUTPUT;
        break;
    case WebMediaPlayerClient::MediaKeyErrorCodeHardwareChange:
        mediaKeyErrorCode = MediaKeyError::MEDIA_KEYERR_HARDWARECHANGE;
        break;
    case WebMediaPlayerClient::MediaKeyErrorCodeDomain:
        mediaKeyErrorCode = MediaKeyError::MEDIA_KEYERR_DOMAIN;
        break;
    }

    MediaKeyEventInit initializer;
    initializer.keySystem = keySystem;
    initializer.sessionId = sessionId;
    initializer.errorCode = MediaKeyError::create(mediaKeyErrorCode);
    initializer.systemCode = systemCode;
    initializer.bubbles = false;
    initializer.cancelable = false;

    RefPtrWillBeRawPtr<Event> event = MediaKeyEvent::create(EventTypeNames::webkitkeyerror, initializer);
    event->setTarget(&element);
    element.scheduleEvent(event.release());
}

void HTMLMediaElementEncryptedMedia::keyMessage(HTMLMediaElement& element, const String& keySystem, const String& sessionId, const unsigned char* message, unsigned messageLength, const WebURL& defaultURL)
{
    WTF_LOG(Media, "HTMLMediaElementEncryptedMedia::mediaPlayerKeyMessage: sessionID=%s", sessionId.utf8().data());

    MediaKeyEventInit initializer;
    initializer.keySystem = keySystem;
    initializer.sessionId = sessionId;
    initializer.message = Uint8Array::create(message, messageLength);
    initializer.defaultURL = KURL(defaultURL);
    initializer.bubbles = false;
    initializer.cancelable = false;

    RefPtrWillBeRawPtr<Event> event = MediaKeyEvent::create(EventTypeNames::webkitkeymessage, initializer);
    event->setTarget(&element);
    element.scheduleEvent(event.release());
}

void HTMLMediaElementEncryptedMedia::keyNeeded(HTMLMediaElement& element, const String& contentType, const unsigned char* initData, unsigned initDataLength)
{
    WTF_LOG(Media, "HTMLMediaElementEncryptedMedia::mediaPlayerKeyNeeded: contentType=%s", contentType.utf8().data());

    if (RuntimeEnabledFeatures::encryptedMediaEnabled()) {
        // Send event for WD EME.
        RefPtrWillBeRawPtr<Event> event = createNeedKeyEvent(contentType, initData, initDataLength);
        event->setTarget(&element);
        element.scheduleEvent(event.release());
    }

    if (RuntimeEnabledFeatures::prefixedEncryptedMediaEnabled()) {
        // Send event for v0.1b EME.
        RefPtrWillBeRawPtr<Event> event = createWebkitNeedKeyEvent(contentType, initData, initDataLength);
        event->setTarget(&element);
        element.scheduleEvent(event.release());
    }
}

void HTMLMediaElementEncryptedMedia::playerDestroyed(HTMLMediaElement& element)
{
#if ENABLE(OILPAN)
    // FIXME: Oilpan: remove this once the media player is on the heap. crbug.com/378229
    if (element.isFinalizing())
        return;
#endif

    HTMLMediaElementEncryptedMedia& thisElement = HTMLMediaElementEncryptedMedia::from(element);
    if (!thisElement.m_mediaKeys)
        return;

    ASSERT(thisElement.m_emeMode == EmeModeUnprefixed);
    thisElement.m_mediaKeys.clear();
}

WebContentDecryptionModule* HTMLMediaElementEncryptedMedia::contentDecryptionModule(HTMLMediaElement& element)
{
    HTMLMediaElementEncryptedMedia& thisElement = HTMLMediaElementEncryptedMedia::from(element);
    return thisElement.contentDecryptionModule();
}

void HTMLMediaElementEncryptedMedia::trace(Visitor* visitor)
{
    visitor->trace(m_mediaKeys);
    WillBeHeapSupplement<HTMLMediaElement>::trace(visitor);
}

} // namespace blink
