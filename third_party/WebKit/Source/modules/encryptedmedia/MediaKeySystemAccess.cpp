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
#include "modules/encryptedmedia/EncryptedMediaUtils.h"
#include "modules/encryptedmedia/MediaKeySession.h"
#include "modules/encryptedmedia/MediaKeys.h"
#include "modules/encryptedmedia/MediaKeysController.h"
#include "platform/Logging.h"
#include "platform/Timer.h"
#include "public/platform/WebContentDecryptionModule.h"
#include "public/platform/WebEncryptedMediaTypes.h"
#include "public/platform/WebMediaKeySystemConfiguration.h"

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
    NewCdmResultPromise(ScriptState* scriptState, const String& keySystem, const blink::WebVector<blink::WebEncryptedMediaSessionType>& supportedSessionTypes)
        : ContentDecryptionModuleResultPromise(scriptState)
        , m_keySystem(keySystem)
        , m_supportedSessionTypes(supportedSessionTypes)
    {
    }

    virtual ~NewCdmResultPromise()
    {
    }

    // ContentDecryptionModuleResult implementation.
    virtual void completeWithContentDecryptionModule(WebContentDecryptionModule* cdm) override
    {
        // NOTE: Continued from step 2.8 of createMediaKeys().
        // 2.9. Let media keys be a new MediaKeys object.
        MediaKeys* mediaKeys = new MediaKeys(executionContext(), m_keySystem, m_supportedSessionTypes, adoptPtr(cdm));

        // 2.10. Resolve promise with media keys.
        resolve(mediaKeys);
    }

private:
    const String m_keySystem;
    blink::WebVector<blink::WebEncryptedMediaSessionType> m_supportedSessionTypes;
};

// These methods are the inverses of those with the same names in
// NavigatorRequestMediaKeySystemAccess.
static Vector<String> convertInitDataTypes(const WebVector<WebEncryptedMediaInitDataType>& initDataTypes)
{
    Vector<String> result;
    result.reserveCapacity(initDataTypes.size());
    for (size_t i = 0; i < initDataTypes.size(); i++)
        result.append(EncryptedMediaUtils::convertFromInitDataType(initDataTypes[i]));
    return result;
}

static Vector<MediaKeySystemMediaCapability> convertCapabilities(const WebVector<WebMediaKeySystemMediaCapability>& capabilities)
{
    Vector<MediaKeySystemMediaCapability> result;
    result.reserveCapacity(capabilities.size());
    for (size_t i = 0; i < capabilities.size(); i++) {
        MediaKeySystemMediaCapability capability;
        capability.setContentType(capabilities[i].contentType);
        capability.setRobustness(capabilities[i].robustness);
        result.append(capability);
    }
    return result;
}

static String convertMediaKeysRequirement(WebMediaKeySystemConfiguration::Requirement requirement)
{
    switch (requirement) {
    case blink::WebMediaKeySystemConfiguration::Requirement::Required:
        return "required";
    case blink::WebMediaKeySystemConfiguration::Requirement::Optional:
        return "optional";
    case blink::WebMediaKeySystemConfiguration::Requirement::NotAllowed:
        return "not-allowed";
    }

    ASSERT_NOT_REACHED();
    return "not-allowed";
}

} // namespace

MediaKeySystemAccess::MediaKeySystemAccess(const String& keySystem, PassOwnPtr<WebContentDecryptionModuleAccess> access)
    : m_keySystem(keySystem)
    , m_access(access)
{
}

MediaKeySystemAccess::~MediaKeySystemAccess()
{
}

void MediaKeySystemAccess::getConfiguration(MediaKeySystemConfiguration& result)
{
    WebMediaKeySystemConfiguration configuration = m_access->getConfiguration();
    result.setInitDataTypes(convertInitDataTypes(configuration.getInitDataTypes()));
    result.setAudioCapabilities(convertCapabilities(configuration.audioCapabilities));
    result.setVideoCapabilities(convertCapabilities(configuration.videoCapabilities));
    result.setDistinctiveIdentifier(convertMediaKeysRequirement(configuration.distinctiveIdentifier));
    result.setPersistentState(convertMediaKeysRequirement(configuration.persistentState));
}

ScriptPromise MediaKeySystemAccess::createMediaKeys(ScriptState* scriptState)
{
    // From http://w3c.github.io/encrypted-media/#createMediaKeys
    // (Reordered to be able to pass values into the promise constructor.)
    // 2.4 Let configuration be the value of this object's configuration value.
    // 2.5-2.8. [Set use distinctive identifier and persistent state allowed
    //          based on configuration.]
    WebMediaKeySystemConfiguration configuration = m_access->getConfiguration();

    // 1. Let promise be a new promise.
    NewCdmResultPromise* helper = new NewCdmResultPromise(scriptState, m_keySystem, configuration.getSessionTypes());
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

DEFINE_TRACE(MediaKeySystemAccess)
{
}

} // namespace blink
