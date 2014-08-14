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
#include "modules/encryptedmedia/MediaKeys.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "modules/encryptedmedia/MediaKeyMessageEvent.h"
#include "modules/encryptedmedia/MediaKeySession.h"
#include "modules/encryptedmedia/MediaKeysController.h"
#include "platform/ContentType.h"
#include "platform/Logging.h"
#include "platform/MIMETypeRegistry.h"
#include "platform/Timer.h"
#include "platform/UUID.h"
#include "public/platform/Platform.h"
#include "public/platform/WebContentDecryptionModule.h"
#include "wtf/ArrayBuffer.h"
#include "wtf/ArrayBufferView.h"
#include "wtf/RefPtr.h"

#if ENABLE(ASSERT)
namespace {

// The list of possible values for |sessionType| passed to createSession().
const char* kTemporary = "temporary";
const char* kPersistent = "persistent";

} // namespace
#endif

namespace blink {

static bool isKeySystemSupportedWithContentType(const String& keySystem, const String& contentType)
{
    ASSERT(!keySystem.isEmpty());

    ContentType type(contentType);
    String codecs = type.parameter("codecs");
    return MIMETypeRegistry::isSupportedEncryptedMediaMIMEType(keySystem, type.type(), codecs);
}

static bool isKeySystemSupportedWithInitDataType(const String& keySystem, const String& initDataType)
{
    // FIXME: initDataType != contentType. Implement this properly.
    // http://crbug.com/385874.
    return isKeySystemSupportedWithContentType(keySystem, initDataType);
}

static ScriptPromise createRejectedPromise(ScriptState* scriptState, ExceptionCode error, const String& errorMessage)
{
    return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(error, errorMessage));
}

// This class allows a MediaKeys object to be created asynchronously.
class MediaKeysInitializer : public ScriptPromiseResolver {
    WTF_MAKE_NONCOPYABLE(MediaKeysInitializer);

public:
    static ScriptPromise create(ScriptState*, const String& keySystem);
    virtual ~MediaKeysInitializer();

private:
    MediaKeysInitializer(ScriptState*, const String& keySystem);
    void timerFired(Timer<MediaKeysInitializer>*);

    const String m_keySystem;
    Timer<MediaKeysInitializer> m_timer;
};

ScriptPromise MediaKeysInitializer::create(ScriptState* scriptState, const String& keySystem)
{
    RefPtr<MediaKeysInitializer> initializer = adoptRef(new MediaKeysInitializer(scriptState, keySystem));
    initializer->suspendIfNeeded();
    initializer->keepAliveWhilePending();
    return initializer->promise();
}

MediaKeysInitializer::MediaKeysInitializer(ScriptState* scriptState, const String& keySystem)
    : ScriptPromiseResolver(scriptState)
    , m_keySystem(keySystem)
    , m_timer(this, &MediaKeysInitializer::timerFired)
{
    WTF_LOG(Media, "MediaKeysInitializer::MediaKeysInitializer");
    // Start the timer so that MediaKeys can be created asynchronously.
    m_timer.startOneShot(0, FROM_HERE);
}

MediaKeysInitializer::~MediaKeysInitializer()
{
    WTF_LOG(Media, "MediaKeysInitializer::~MediaKeysInitializer");
}

void MediaKeysInitializer::timerFired(Timer<MediaKeysInitializer>*)
{
    WTF_LOG(Media, "MediaKeysInitializer::timerFired");

    // NOTE: Continued from step 4. of MediaKeys::create().
    // 4.1 Let cdm be the content decryption module corresponding to
    //     keySystem.
    // 4.2 Load and initialize the cdm if necessary.
    Document* document = toDocument(executionContext());
    MediaKeysController* controller = MediaKeysController::from(document->page());
    // FIXME: make createContentDecryptionModule() asynchronous.
    OwnPtr<WebContentDecryptionModule> cdm = controller->createContentDecryptionModule(executionContext(), m_keySystem);

    // 4.3 If cdm fails to load or initialize, reject promise with a new
    //     DOMException whose name is the appropriate error name and that
    //     has an appropriate message.
    if (!cdm) {
        String message("A content decryption module could not be loaded for the '" + m_keySystem + "' key system.");
        reject(DOMException::create(UnknownError, message));
        return;
    }

    // 4.4 Let media keys be a new MediaKeys object.
    MediaKeys* mediaKeys = new MediaKeys(executionContext(), m_keySystem, cdm.release());

    // 4.5. Resolve promise with media keys.
    resolve(mediaKeys);

    // Note: As soon as the promise is resolved (or rejected), the
    // ScriptPromiseResolver object (|this|) is freed. So access to
    // any members will crash once the promise is fulfilled.
}

ScriptPromise MediaKeys::create(ScriptState* scriptState, const String& keySystem)
{
    WTF_LOG(Media, "MediaKeys::create(%s)", keySystem.ascii().data());

    // From https://dvcs.w3.org/hg/html-media/raw-file/default/encrypted-media/encrypted-media.html#dom-create:
    // The create(keySystem) method creates a new MediaKeys object for keySystem. It must run the following steps:

    // 1. If keySystem is an empty string, return a promise rejected with a new
    // DOMException whose name is "InvalidAccessError" and that has the message
    // "The keySystem parameter is empty."
    if (keySystem.isEmpty()) {
        return createRejectedPromise(scriptState, InvalidAccessError, "The keySystem parameter is empty.");
    }

    // 2. If keySystem is not one of the Key Systems supported by the user
    // agent, return a promise rejected with a new DOMException whose name is
    // "NotSupportedError" and that has the message "The key system keySystem
    // is not supported." String comparison is case-sensitive.
    if (!isKeySystemSupportedWithContentType(keySystem, "")) {
        // String message("The key system '" + keySystem + "' is not supported.");
        return createRejectedPromise(scriptState, NotSupportedError, "The key system '" + keySystem + "' is not supported.");
    }

    // 3. Let promise be a new promise.
    // 4. Asynchronously create and initialize the MediaKeys.
    // 5. Return promise.
    return MediaKeysInitializer::create(scriptState, keySystem);
}

MediaKeys::MediaKeys(ExecutionContext* context, const String& keySystem, PassOwnPtr<WebContentDecryptionModule> cdm)
    : ContextLifecycleObserver(context)
    , m_keySystem(keySystem)
    , m_cdm(cdm)
{
    WTF_LOG(Media, "MediaKeys(%p)::MediaKeys", this);
    ScriptWrappable::init(this);

    // Step 4.4 of MediaKeys::create():
    // 4.4.1 Set the keySystem attribute to keySystem.
    ASSERT(!m_keySystem.isEmpty());
}

MediaKeys::~MediaKeys()
{
    WTF_LOG(Media, "MediaKeys(%p)::~MediaKeys", this);
}

ScriptPromise MediaKeys::createSession(ScriptState* scriptState, const String& initDataType, ArrayBuffer* initData, const String& sessionType)
{
    RefPtr<ArrayBuffer> initDataCopy = ArrayBuffer::create(initData->data(), initData->byteLength());
    return createSessionInternal(scriptState, initDataType, initDataCopy.release(), sessionType);
}

ScriptPromise MediaKeys::createSession(ScriptState* scriptState, const String& initDataType, ArrayBufferView* initData, const String& sessionType)
{
    RefPtr<ArrayBuffer> initDataCopy = ArrayBuffer::create(initData->baseAddress(), initData->byteLength());
    return createSessionInternal(scriptState, initDataType, initDataCopy.release(), sessionType);
}

ScriptPromise MediaKeys::createSessionInternal(ScriptState* scriptState, const String& initDataType, PassRefPtr<ArrayBuffer> initData, const String& sessionType)
{
    WTF_LOG(Media, "MediaKeys(%p)::createSession(%s, %d)", this, initDataType.ascii().data(), initData->byteLength());

    // From <http://dvcs.w3.org/hg/html-media/raw-file/default/encrypted-media/encrypted-media.html#dom-createsession>:
    // The createSession(initDataType, initData, sessionType) method creates a
    // new MediaKeySession object for the initData. It must run the following steps:

    // 1. If initDataType is an empty string, return a promise rejected with a
    //    new DOMException whose name is "InvalidAccessError".
    if (initDataType.isEmpty()) {
        return createRejectedPromise(scriptState, InvalidAccessError, "The initDataType parameter is empty.");
    }

    // 2. If initData is an empty array, return a promise rejected with a new
    //    DOMException whose name is"InvalidAccessError".
    if (!initData->byteLength()) {
        return createRejectedPromise(scriptState, InvalidAccessError, "The initData parameter is empty.");
    }

    // 3. If initDataType is not an initialization data type supported by the
    //    content decryption module corresponding to the keySystem, return a
    //    promise rejected with a new DOMException whose name is
    //    "NotSupportedError". String comparison is case-sensitive.
    if (!isKeySystemSupportedWithInitDataType(m_keySystem, initDataType)) {
        return createRejectedPromise(scriptState, NotSupportedError, "The initialization data type '" + initDataType + "' is not supported by the key system.");
    }

    // 4. If sessionType is not supported by the content decryption module
    //    corresponding to the keySystem, return a promise rejected with a new
    //    DOMException whose name is "NotSupportedError".
    //    Since this is typed by the IDL, we should not see any invalid values.
    // FIXME: Check whether sessionType is actually supported by the CDM.
    ASSERT(sessionType == kTemporary || sessionType == kPersistent);

    // 5. Let init data be a copy of the contents of the initData parameter.
    //    (Copied in the caller.)
    // 6. Let promise be a new promise.
    // 7. Asynchronously create and initialize the session.
    // 8. Return promise.
    return MediaKeySession::create(scriptState, this, initDataType, initData, sessionType);
}

bool MediaKeys::isTypeSupported(const String& keySystem, const String& contentType)
{
    WTF_LOG(Media, "MediaKeys::isTypeSupported(%s, %s)", keySystem.ascii().data(), contentType.ascii().data());

    // 1. If keySystem is an empty string, return false and abort these steps.
    if (keySystem.isEmpty())
        return false;

    // 2. If keySystem contains an unrecognized or unsupported Key System, return false and abort
    // these steps. Key system string comparison is case-sensitive.
    if (!isKeySystemSupportedWithContentType(keySystem, ""))
        return false;

    // 3. If contentType is an empty string, return true and abort these steps.
    if (contentType.isEmpty())
        return true;

    // 4. If the Key System specified by keySystem does not support decrypting the container and/or
    // codec specified by contentType, return false and abort these steps.
    return isKeySystemSupportedWithContentType(keySystem, contentType);
}

WebContentDecryptionModule* MediaKeys::contentDecryptionModule()
{
    return m_cdm.get();
}

void MediaKeys::trace(Visitor* visitor)
{
}

void MediaKeys::contextDestroyed()
{
    ContextLifecycleObserver::contextDestroyed();

    // We don't need the CDM anymore.
    m_cdm.clear();
}

} // namespace blink
