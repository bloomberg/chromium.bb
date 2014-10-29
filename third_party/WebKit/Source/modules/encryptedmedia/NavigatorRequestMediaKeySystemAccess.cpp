// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/encryptedmedia/NavigatorRequestMediaKeySystemAccess.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "modules/encryptedmedia/MediaKeySystemAccess.h"
#include "platform/ContentType.h"
#include "platform/Logging.h"
#include "platform/MIMETypeRegistry.h"
#include "wtf/text/WTFString.h"

namespace {

static bool isKeySystemSupportedWithContentType(const String& keySystem, const String& contentType)
{
    ASSERT(!keySystem.isEmpty());

    blink::ContentType type(contentType);
    String codecs = type.parameter("codecs");
    return blink::MIMETypeRegistry::isSupportedEncryptedMediaMIMEType(keySystem, type.type(), codecs);
}

// This class allows capabilities to be checked and a MediaKeySystemAccess
// object to be created asynchronously.
class MediaKeySystemAccessInitializer final : public blink::ScriptPromiseResolver {
    WTF_MAKE_NONCOPYABLE(MediaKeySystemAccessInitializer);

public:
    static blink::ScriptPromise create(blink::ScriptState*, const String& keySystem, const Vector<blink::MediaKeySystemOptions>& supportedConfigurations);
    virtual ~MediaKeySystemAccessInitializer();

private:
    MediaKeySystemAccessInitializer(blink::ScriptState*, const String& keySystem, const Vector<blink::MediaKeySystemOptions>& supportedConfigurations);
    void timerFired(blink::Timer<MediaKeySystemAccessInitializer>*);

    const String m_keySystem;
    const Vector<blink::MediaKeySystemOptions> m_supportedConfigurations;
    blink::Timer<MediaKeySystemAccessInitializer> m_timer;
};

blink::ScriptPromise MediaKeySystemAccessInitializer::create(blink::ScriptState* scriptState, const String& keySystem, const Vector<blink::MediaKeySystemOptions>& supportedConfigurations)
{
    RefPtr<MediaKeySystemAccessInitializer> initializer = adoptRef(new MediaKeySystemAccessInitializer(scriptState, keySystem, supportedConfigurations));
    initializer->suspendIfNeeded();
    initializer->keepAliveWhilePending();
    return initializer->promise();
}

MediaKeySystemAccessInitializer::MediaKeySystemAccessInitializer(blink::ScriptState* scriptState, const String& keySystem, const Vector<blink::MediaKeySystemOptions>& supportedConfigurations)
    : blink::ScriptPromiseResolver(scriptState)
    , m_keySystem(keySystem)
    , m_supportedConfigurations(supportedConfigurations)
    , m_timer(this, &MediaKeySystemAccessInitializer::timerFired)
{
    // Start the timer so that MediaKeySystemAccess can be created
    // asynchronously.
    // FIXME: If creating MediaKeySystemAccess or
    // isKeySystemSupportedWithContentType() is replaced with something
    // asynchronous, wait for the event rather than using a timer.
    m_timer.startOneShot(0, FROM_HERE);
}

MediaKeySystemAccessInitializer::~MediaKeySystemAccessInitializer()
{
}

void MediaKeySystemAccessInitializer::timerFired(blink::Timer<MediaKeySystemAccessInitializer>*)
{
    // Continued from requestMediaKeySystemAccess().
    // 5.1 If keySystem is not supported or not allowed in the origin of the
    //     calling context's Document, return a promise rejected with a new
    //     DOMException whose name is NotSupportedError.
    //     (Handled by Chromium.)

    // 5.2 If supportedConfigurations was not provided, resolve the promise
    //     with a new MediaKeySystemAccess object, execute the following steps:
    if (!m_supportedConfigurations.size()) {
        // 5.2.1 Let access be a new MediaKeySystemAccess object, and initialize
        //       it as follows:
        // 5.2.1.1 Set the keySystem attribute to keySystem.
        blink::MediaKeySystemAccess* access = new blink::MediaKeySystemAccess(m_keySystem);

        // 5.2.2 Resolve promise with access and abort these steps.
        resolve(access);
        return;
    }

    // 5.3 For each element of supportedConfigurations:
    // 5.3.1 Let combination be the element.
    // 5.3.2 For each dictionary member in combination:
    for (const auto& combination : m_supportedConfigurations) {
        // 5.3.2.1 If the member's value cannot be satisfied together in
        //         combination with the previous members, continue to the next
        //         iteration of the loop.
        // 5.3.3 If keySystem is supported and allowed in the origin of the
        //       calling context's Document in the configuration specified by
        //       the combination of the values in combination, execute the
        //       following steps:
        // FIXME: This test needs to be enhanced to use more values from
        //        combination.
        if (isKeySystemSupportedWithContentType(m_keySystem, combination.initDataType())) {
            // 5.3.3.1 Let access be a new MediaKeySystemAccess object, and
            //         initialize it as follows:
            // 5.3.3.1.1 Set the keySystem attribute to keySystem.
            blink::MediaKeySystemAccess* access = new blink::MediaKeySystemAccess(m_keySystem);

            // 5.3.3.2 Resolve promise with access and abort these steps.
            resolve(access);
            return;
        }
    }

    // 5.4 Reject promise with a new DOMException whose name is
    //     NotSupportedError.
    reject(blink::DOMException::create(blink::NotSupportedError, "There were no supported combinations in supportedConfigurations."));
}

} // namespace

namespace blink {

NavigatorRequestMediaKeySystemAccess::NavigatorRequestMediaKeySystemAccess()
{
}

NavigatorRequestMediaKeySystemAccess::~NavigatorRequestMediaKeySystemAccess()
{
}

NavigatorRequestMediaKeySystemAccess& NavigatorRequestMediaKeySystemAccess::from(Navigator& navigator)
{
    NavigatorRequestMediaKeySystemAccess* supplement = static_cast<NavigatorRequestMediaKeySystemAccess*>(WillBeHeapSupplement<Navigator>::from(navigator, supplementName()));
    if (!supplement) {
        supplement = new NavigatorRequestMediaKeySystemAccess();
        provideTo(navigator, supplementName(), adoptPtrWillBeNoop(supplement));
    }
    return *supplement;
}

ScriptPromise NavigatorRequestMediaKeySystemAccess::requestMediaKeySystemAccess(
    ScriptState* scriptState,
    Navigator& navigator,
    const String& keySystem)
{
    // From https://dvcs.w3.org/hg/html-media/raw-file/default/encrypted-media/encrypted-media.html#requestmediakeysystemaccess
    // When this method is invoked, the user agent must run the following steps:
    // 1. If keySystem is an empty string, return a promise rejected with a
    //    new DOMException whose name is InvalidAccessError.
    if (keySystem.isEmpty()) {
        return ScriptPromise::rejectWithDOMException(
            scriptState, DOMException::create(InvalidAccessError, "The keySystem parameter is empty."));
    }

    // 2. If supportedConfigurations was provided and is empty, return a
    //    promise rejected with a new DOMException whose name is
    //    InvalidAccessError.
    //    (no supportedConfigurations provided.)

    // Remainder of steps handled in common routine below.
    return NavigatorRequestMediaKeySystemAccess::from(navigator).requestMediaKeySystemAccess(scriptState, keySystem, Vector<MediaKeySystemOptions>());
}

ScriptPromise NavigatorRequestMediaKeySystemAccess::requestMediaKeySystemAccess(
    ScriptState* scriptState,
    Navigator& navigator,
    const String& keySystem,
    const Vector<MediaKeySystemOptions>& supportedConfigurations)
{
    // From https://dvcs.w3.org/hg/html-media/raw-file/default/encrypted-media/encrypted-media.html#requestmediakeysystemaccess
    // When this method is invoked, the user agent must run the following steps:
    // 1. If keySystem is an empty string, return a promise rejected with a
    //    new DOMException whose name is InvalidAccessError.
    if (keySystem.isEmpty()) {
        return ScriptPromise::rejectWithDOMException(
            scriptState, DOMException::create(InvalidAccessError, "The keySystem parameter is empty."));
    }

    // 2. If supportedConfigurations was provided and is empty, return a
    //    promise rejected with a new DOMException whose name is
    //    InvalidAccessError.
    if (!supportedConfigurations.size()) {
        return ScriptPromise::rejectWithDOMException(
            scriptState, DOMException::create(InvalidAccessError, "The supportedConfigurations parameter is empty."));
    }

    // Remainder of steps handled in common routine below.
    return NavigatorRequestMediaKeySystemAccess::from(navigator).requestMediaKeySystemAccess(
        scriptState, keySystem, supportedConfigurations);
}

ScriptPromise NavigatorRequestMediaKeySystemAccess::requestMediaKeySystemAccess(
    ScriptState* scriptState,
    const String& keySystem,
    const Vector<MediaKeySystemOptions>& supportedConfigurations)
{
    // Continued from above.
    // 3. If keySystem is not one of the Key Systems supported by the user
    //    agent, return a promise rejected with a new DOMException whose name
    //    is NotSupportedError. String comparison is case-sensitive.
    if (!isKeySystemSupportedWithContentType(keySystem, "")) {
        return ScriptPromise::rejectWithDOMException(
            scriptState, DOMException::create(NotSupportedError, "The key system specified is not supported."));
    }

    // 4. Let promise be a new promise.
    // 5. Asynchronously determine support, and if allowed, create and
    //    initialize the MediaKeySystemAccess object.
    // 6. Return promise.
    return MediaKeySystemAccessInitializer::create(scriptState, keySystem, supportedConfigurations);
}

const char* NavigatorRequestMediaKeySystemAccess::supplementName()
{
    return "RequestMediaKeySystemAccess";
}

void NavigatorRequestMediaKeySystemAccess::trace(Visitor* visitor)
{
    WillBeHeapSupplement<Navigator>::trace(visitor);
}

} // namespace blink
