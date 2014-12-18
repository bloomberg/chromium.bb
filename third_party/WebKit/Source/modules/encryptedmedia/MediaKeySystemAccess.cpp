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
#include "modules/encryptedmedia/ContentDecryptionModuleResultPromise.h"
#include "modules/encryptedmedia/MediaKeys.h"
#include "modules/encryptedmedia/MediaKeysController.h"
#include "platform/Logging.h"
#include "platform/Timer.h"
#include "public/platform/WebContentDecryptionModule.h"

namespace blink {

namespace {

// This class wraps the promise resolver used when creating MediaKeys
// and is passed to Chromium to fullfill the promise. This implementation of
// completeWithCdm() will resolve the promise with a new MediaKeys object,
// while completeWithError() will reject the promise with an exception.
// All other complete methods are not expected to be called, and will
// reject the promise.
class NewCdmResultPromise : public ContentDecryptionModuleResultPromise {
    WTF_MAKE_NONCOPYABLE(NewCdmResultPromise);

public:
    NewCdmResultPromise(ScriptState* scriptState, const String& keySystem)
        : ContentDecryptionModuleResultPromise(scriptState)
        , m_keySystem(keySystem)
    {
    }

    virtual ~NewCdmResultPromise()
    {
    }

    // ContentDecryptionModuleResult implementation.
    virtual void completeWithContentDecryptionModule(WebContentDecryptionModule* cdm) override
    {
        // NOTE: Continued from step 2. of createMediaKeys().
        // 2.4 Let media keys be a new MediaKeys object.
        MediaKeys* mediaKeys = new MediaKeys(executionContext(), m_keySystem, adoptPtr(cdm));

        // 2.5 Resolve promise with media keys.
        resolve(mediaKeys);
    }

private:
    const String m_keySystem;
};

} // namespace

MediaKeySystemAccess::MediaKeySystemAccess(const String& keySystem, PassOwnPtr<WebContentDecryptionModuleAccess> access)
    : m_keySystem(keySystem)
    , m_access(access)
{
}

MediaKeySystemAccess::~MediaKeySystemAccess()
{
}

ScriptPromise MediaKeySystemAccess::createMediaKeys(ScriptState* scriptState)
{
    // From https://dvcs.w3.org/hg/html-media/raw-file/default/encrypted-media/encrypted-media.html#widl-MediaKeySystemAccess-createMediaKeys-Promise-MediaKeys
    // When this method is invoked, the user agent must run the following steps:
    // 1. Let promise be a new promise.
    NewCdmResultPromise* helper = new NewCdmResultPromise(scriptState, m_keySystem);
    ScriptPromise promise = helper->promise();

    // 2. Asynchronously create and initialize the MediaKeys object.
    // 2.1 Let cdm be the CDM corresponding to this object.
    // 2.2 Load and initialize the cdm if necessary.
    // 2.3 If cdm fails to load or initialize, reject promise with a new
    //     DOMException whose name is the appropriate error name.
    //     (Done if completeWithException() called).
    m_access->createContentDecryptionModule(helper->result());

    // 3. Return promise.
    return promise;
}

void MediaKeySystemAccess::trace(Visitor*)
{
}

} // namespace blink
