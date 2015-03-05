// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/encryptedmedia/NavigatorRequestMediaKeySystemAccess.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "modules/encryptedmedia/EncryptedMediaRequest.h"
#include "modules/encryptedmedia/EncryptedMediaUtils.h"
#include "modules/encryptedmedia/MediaKeySession.h"
#include "modules/encryptedmedia/MediaKeySystemAccess.h"
#include "modules/encryptedmedia/MediaKeysController.h"
#include "platform/Logging.h"
#include "platform/network/ParsedContentType.h"
#include "public/platform/WebEncryptedMediaClient.h"
#include "public/platform/WebEncryptedMediaRequest.h"
#include "public/platform/WebMediaKeySystemConfiguration.h"
#include "public/platform/WebMediaKeySystemMediaCapability.h"
#include "public/platform/WebVector.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

namespace {

static WebVector<WebEncryptedMediaInitDataType> convertInitDataTypes(const Vector<String>& initDataTypes)
{
    WebVector<WebEncryptedMediaInitDataType> result(initDataTypes.size());
    for (size_t i = 0; i < initDataTypes.size(); ++i)
        result[i] = EncryptedMediaUtils::convertToInitDataType(initDataTypes[i]);
    return result;
}

static WebVector<WebMediaKeySystemMediaCapability> convertCapabilities(const Vector<MediaKeySystemMediaCapability>& capabilities)
{
    WebVector<WebMediaKeySystemMediaCapability> result(capabilities.size());
    for (size_t i = 0; i < capabilities.size(); ++i) {
        const WebString& contentType = capabilities[i].contentType();
        result[i].contentType = contentType;
        if (isValidContentType(contentType)) {
            // FIXME: Fail if there are unrecognized parameters.
            ParsedContentType type(capabilities[i].contentType());
            result[i].mimeType = type.mimeType();
            result[i].codecs = type.parameterValueForName("codecs");
        }
        result[i].robustness = capabilities[i].robustness();
    }
    return result;
}

static WebMediaKeySystemConfiguration::Requirement convertMediaKeysRequirement(const String& requirement)
{
    if (requirement == "required")
        return WebMediaKeySystemConfiguration::Requirement::Required;
    if (requirement == "optional")
        return WebMediaKeySystemConfiguration::Requirement::Optional;
    if (requirement == "not-allowed")
        return WebMediaKeySystemConfiguration::Requirement::NotAllowed;

    // Everything else gets the default value.
    ASSERT_NOT_REACHED();
    return WebMediaKeySystemConfiguration::Requirement::Optional;
}

// This class allows capabilities to be checked and a MediaKeySystemAccess
// object to be created asynchronously.
class MediaKeySystemAccessInitializer final : public EncryptedMediaRequest {
    WTF_MAKE_NONCOPYABLE(MediaKeySystemAccessInitializer);

public:
    MediaKeySystemAccessInitializer(ScriptState*, const String& keySystem, const Vector<MediaKeySystemConfiguration>& supportedConfigurations);
    virtual ~MediaKeySystemAccessInitializer() { }

    // EncryptedMediaRequest implementation.
    virtual WebString keySystem() const override { return m_keySystem; }
    virtual const WebVector<WebMediaKeySystemConfiguration>& supportedConfigurations() const override { return m_supportedConfigurations; }
    virtual SecurityOrigin* securityOrigin() const override { return m_resolver->executionContext()->securityOrigin(); }
    virtual void requestSucceeded(WebContentDecryptionModuleAccess*) override;
    virtual void requestNotSupported(const WebString& errorMessage) override;

    ScriptPromise promise() { return m_resolver->promise(); }

private:
    RefPtr<ScriptPromiseResolver> m_resolver;
    const String m_keySystem;
    WebVector<WebMediaKeySystemConfiguration> m_supportedConfigurations;
};

MediaKeySystemAccessInitializer::MediaKeySystemAccessInitializer(ScriptState* scriptState, const String& keySystem, const Vector<MediaKeySystemConfiguration>& supportedConfigurations)
    : m_resolver(ScriptPromiseResolver::create(scriptState))
    , m_keySystem(keySystem)
    , m_supportedConfigurations(supportedConfigurations.size())
{
    for (size_t i = 0; i < supportedConfigurations.size(); ++i) {
        const MediaKeySystemConfiguration& config = supportedConfigurations[i];
        WebMediaKeySystemConfiguration webConfig;
        if (config.hasInitDataTypes())
            webConfig.initDataTypes = convertInitDataTypes(config.initDataTypes());
        if (config.hasAudioCapabilities())
            webConfig.audioCapabilities = convertCapabilities(config.audioCapabilities());
        if (config.hasVideoCapabilities())
            webConfig.videoCapabilities = convertCapabilities(config.videoCapabilities());
        ASSERT(config.hasDistinctiveIdentifier());
        webConfig.distinctiveIdentifier = convertMediaKeysRequirement(config.distinctiveIdentifier());
        ASSERT(config.hasPersistentState());
        webConfig.persistentState = convertMediaKeysRequirement(config.persistentState());
        m_supportedConfigurations[i] = webConfig;
    }
}

void MediaKeySystemAccessInitializer::requestSucceeded(WebContentDecryptionModuleAccess* access)
{
    m_resolver->resolve(new MediaKeySystemAccess(m_keySystem, adoptPtr(access)));
    m_resolver.clear();
}

void MediaKeySystemAccessInitializer::requestNotSupported(const WebString& errorMessage)
{
    m_resolver->reject(DOMException::create(NotSupportedError, errorMessage));
    m_resolver.clear();
}

} // namespace

ScriptPromise NavigatorRequestMediaKeySystemAccess::requestMediaKeySystemAccess(
    ScriptState* scriptState,
    Navigator& navigator,
    const String& keySystem,
    const Vector<MediaKeySystemConfiguration>& supportedConfigurations)
{
    WTF_LOG(Media, "NavigatorRequestMediaKeySystemAccess::requestMediaKeySystemAccess()");

    // From https://w3c.github.io/encrypted-media/#requestMediaKeySystemAccess
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

    // 3-4. 'May Document use powerful features?' check.
    // FIXME: Implement 'May Document use powerful features?' check.

    // 5. Let origin be the origin of document.
    //    (Passed with the execution context in step 7.)

    // 6. Let promise be a new promise.
    Document* document = toDocument(scriptState->executionContext());
    if (!document->page()) {
        return ScriptPromise::rejectWithDOMException(
            scriptState, DOMException::create(InvalidStateError, "Document does not have a page."));
    }

    MediaKeySystemAccessInitializer* initializer = new MediaKeySystemAccessInitializer(scriptState, keySystem, supportedConfigurations);
    ScriptPromise promise = initializer->promise();

    // 7. Asynchronously determine support, and if allowed, create and
    //    initialize the MediaKeySystemAccess object.
    MediaKeysController* controller = MediaKeysController::from(document->page());
    WebEncryptedMediaClient* mediaClient = controller->encryptedMediaClient(scriptState->executionContext());
    mediaClient->requestMediaKeySystemAccess(WebEncryptedMediaRequest(initializer));

    // 8. Return promise.
    return promise;
}

} // namespace blink
