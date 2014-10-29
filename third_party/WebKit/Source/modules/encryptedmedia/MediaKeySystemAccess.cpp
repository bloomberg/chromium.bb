// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/encryptedmedia/MediaKeySystemAccess.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "modules/encryptedmedia/MediaKeys.h"
#include "modules/encryptedmedia/MediaKeysController.h"
#include "platform/Logging.h"
#include "platform/Timer.h"
#include "public/platform/WebContentDecryptionModule.h"

namespace {

// This class allows a MediaKeys object to be created asynchronously.
class MediaKeysInitializer final : public blink::ScriptPromiseResolver {
    WTF_MAKE_NONCOPYABLE(MediaKeysInitializer);

public:
    static blink::ScriptPromise create(blink::ScriptState*, const String& keySystem);
    virtual ~MediaKeysInitializer();

private:
    MediaKeysInitializer(blink::ScriptState*, const String& keySystem);
    void timerFired(blink::Timer<MediaKeysInitializer>*);

    const String m_keySystem;
    blink::Timer<MediaKeysInitializer> m_timer;
};

blink::ScriptPromise MediaKeysInitializer::create(blink::ScriptState* scriptState, const String& keySystem)
{
    RefPtr<MediaKeysInitializer> initializer = adoptRef(new MediaKeysInitializer(scriptState, keySystem));
    initializer->suspendIfNeeded();
    initializer->keepAliveWhilePending();
    return initializer->promise();
}

MediaKeysInitializer::MediaKeysInitializer(blink::ScriptState* scriptState, const String& keySystem)
    : blink::ScriptPromiseResolver(scriptState)
    , m_keySystem(keySystem)
    , m_timer(this, &MediaKeysInitializer::timerFired)
{
    // Start the timer so that MediaKeys can be created asynchronously.
    // FIXME: When createContentDecryptionModule() is made asynchronous, wait
    // for the event rather than using a timer.
    m_timer.startOneShot(0, FROM_HERE);
}

MediaKeysInitializer::~MediaKeysInitializer()
{
}

void MediaKeysInitializer::timerFired(blink::Timer<MediaKeysInitializer>*)
{
    // NOTE: Continued from step 2. of MediaKeySystemAccess::createMediaKeys().
    // 2.1 Let cdm be the CDM corresponding to this object.
    // 2.2 Load and initialize the cdm if necessary.
    blink::Document* document = toDocument(executionContext());
    blink::MediaKeysController* controller = blink::MediaKeysController::from(document->page());
    // FIXME: Should this return an error code so there can be a better error
    // message than UnknownError? Should it be asynchronous (maybe use
    // webContentDecryptionModuleResult)?
    OwnPtr<blink::WebContentDecryptionModule> cdm = controller->createContentDecryptionModule(executionContext(), m_keySystem);

    // 2.3 If cdm fails to load or initialize, reject promise with a new
    //     DOMException whose name is the appropriate error name.
    if (!cdm) {
        String message("A CDM instance could not be created for the '" + m_keySystem + "' key system.");
        reject(blink::DOMException::create(blink::UnknownError, message));
        return;
    }

    // 2.4 Let media keys be a new MediaKeys object.
    blink::MediaKeys* mediaKeys = new blink::MediaKeys(executionContext(), m_keySystem, cdm.release());

    // 2.5 Resolve promise with media keys.
    resolve(mediaKeys);

    // Note: As soon as the promise is resolved (or rejected), the
    // ScriptPromiseResolver object (|this|) is freed. So access to
    // any members will crash once the promise is fulfilled.
}

} // namespace

namespace blink {

MediaKeySystemAccess::MediaKeySystemAccess(const String& keySystem)
    : m_keySystem(keySystem)
{
    // FIXME: There should be a Chromium-side equivalent object that contains
    // any information, including the key system, from the results of the
    // request call. It would also give us a place to put the createCDM() call.
}

MediaKeySystemAccess::~MediaKeySystemAccess()
{
}

ScriptPromise MediaKeySystemAccess::createMediaKeys(ScriptState* scriptState)
{
    // From https://dvcs.w3.org/hg/html-media/raw-file/default/encrypted-media/encrypted-media.html#widl-MediaKeySystemAccess-createMediaKeys-Promise-MediaKeys
    // When this method is invoked, the user agent must run the following steps:
    // 1. Let promise be a new promise.
    // 2. Asynchronously create and initialize the MediaKeys object.
    // 3. Return promise.
    return MediaKeysInitializer::create(scriptState, m_keySystem);
}

void MediaKeySystemAccess::trace(Visitor*)
{
}

} // namespace blink
