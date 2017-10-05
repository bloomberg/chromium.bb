// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/encryptedmedia/NavigatorRequestMediaKeySystemAccess.h"

#include <algorithm>

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/Deprecation.h"
#include "core/frame/Settings.h"
#include "core/inspector/ConsoleMessage.h"
#include "modules/encryptedmedia/EncryptedMediaUtils.h"
#include "modules/encryptedmedia/MediaKeySession.h"
#include "modules/encryptedmedia/MediaKeySystemAccess.h"
#include "modules/encryptedmedia/MediaKeysController.h"
#include "platform/EncryptedMediaRequest.h"
#include "platform/Histogram.h"
#include "platform/bindings/ScriptState.h"
#include "platform/bindings/V8ThrowException.h"
#include "platform/network/ParsedContentType.h"
#include "platform/network/mime/ContentType.h"
#include "platform/runtime_enabled_features.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebEncryptedMediaClient.h"
#include "public/platform/WebEncryptedMediaRequest.h"
#include "public/platform/WebFeaturePolicyFeature.h"
#include "public/platform/WebMediaKeySystemConfiguration.h"
#include "public/platform/WebMediaKeySystemMediaCapability.h"
#include "public/platform/WebVector.h"

namespace blink {

namespace {

static WebVector<WebEncryptedMediaInitDataType> ConvertInitDataTypes(
    const Vector<String>& init_data_types) {
  WebVector<WebEncryptedMediaInitDataType> result(init_data_types.size());
  for (size_t i = 0; i < init_data_types.size(); ++i)
    result[i] = EncryptedMediaUtils::ConvertToInitDataType(init_data_types[i]);
  return result;
}

static WebVector<WebMediaKeySystemMediaCapability> ConvertCapabilities(
    const HeapVector<MediaKeySystemMediaCapability>& capabilities) {
  WebVector<WebMediaKeySystemMediaCapability> result(capabilities.size());
  for (size_t i = 0; i < capabilities.size(); ++i) {
    const WebString& content_type = capabilities[i].contentType();
    result[i].content_type = content_type;
    ParsedContentType type(content_type, ParsedContentType::Mode::kStrict);
    if (type.IsValid()) {
      // From
      // http://w3c.github.io/encrypted-media/#get-supported-capabilities-for-audio-video-type
      // "If the user agent does not recognize one or more parameters,
      // continue to the next iteration." There is no way to enumerate the
      // parameters, so only look up "codecs" if a single parameter is
      // present. Chromium expects "codecs" to be provided, so this capability
      // will be skipped if codecs is not the only parameter specified.
      result[i].mime_type = type.MimeType();
      if (type.ParameterCount() == 1u)
        result[i].codecs = type.ParameterValueForName("codecs");
    }
    result[i].robustness = capabilities[i].robustness();
  }
  return result;
}

static WebMediaKeySystemConfiguration::Requirement ConvertMediaKeysRequirement(
    const String& requirement) {
  if (requirement == "required")
    return WebMediaKeySystemConfiguration::Requirement::kRequired;
  if (requirement == "optional")
    return WebMediaKeySystemConfiguration::Requirement::kOptional;
  if (requirement == "not-allowed")
    return WebMediaKeySystemConfiguration::Requirement::kNotAllowed;

  // Everything else gets the default value.
  NOTREACHED();
  return WebMediaKeySystemConfiguration::Requirement::kOptional;
}

static WebVector<WebEncryptedMediaSessionType> ConvertSessionTypes(
    const Vector<String>& session_types) {
  WebVector<WebEncryptedMediaSessionType> result(session_types.size());
  for (size_t i = 0; i < session_types.size(); ++i)
    result[i] = EncryptedMediaUtils::ConvertToSessionType(session_types[i]);
  return result;
}

// This class allows capabilities to be checked and a MediaKeySystemAccess
// object to be created asynchronously.
class MediaKeySystemAccessInitializer final : public EncryptedMediaRequest {
  WTF_MAKE_NONCOPYABLE(MediaKeySystemAccessInitializer);

 public:
  MediaKeySystemAccessInitializer(
      ScriptState*,
      const String& key_system,
      const HeapVector<MediaKeySystemConfiguration>& supported_configurations);
  ~MediaKeySystemAccessInitializer() override {}

  // EncryptedMediaRequest implementation.
  WebString KeySystem() const override { return key_system_; }
  const WebVector<WebMediaKeySystemConfiguration>& SupportedConfigurations()
      const override {
    return supported_configurations_;
  }
  SecurityOrigin* GetSecurityOrigin() const override;
  void RequestSucceeded(WebContentDecryptionModuleAccess*) override;
  void RequestNotSupported(const WebString& error_message) override;

  ScriptPromise Promise() { return resolver_->Promise(); }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(resolver_);
    EncryptedMediaRequest::Trace(visitor);
  }

 private:
  // Returns true if the ExecutionContext is valid, false otherwise.
  bool IsExecutionContextValid() const;

  // For widevine key system, generate warning and report to UMA if
  // |m_supportedConfigurations| contains any video capability with empty
  // robustness string.
  void CheckVideoCapabilityRobustness() const;

  Member<ScriptPromiseResolver> resolver_;
  const String key_system_;
  WebVector<WebMediaKeySystemConfiguration> supported_configurations_;
};

MediaKeySystemAccessInitializer::MediaKeySystemAccessInitializer(
    ScriptState* script_state,
    const String& key_system,
    const HeapVector<MediaKeySystemConfiguration>& supported_configurations)
    : resolver_(ScriptPromiseResolver::Create(script_state)),
      key_system_(key_system),
      supported_configurations_(supported_configurations.size()) {
  for (size_t i = 0; i < supported_configurations.size(); ++i) {
    const MediaKeySystemConfiguration& config = supported_configurations[i];
    WebMediaKeySystemConfiguration web_config;

    DCHECK(config.hasInitDataTypes());
    web_config.init_data_types = ConvertInitDataTypes(config.initDataTypes());

    DCHECK(config.hasAudioCapabilities());
    web_config.audio_capabilities =
        ConvertCapabilities(config.audioCapabilities());

    DCHECK(config.hasVideoCapabilities());
    web_config.video_capabilities =
        ConvertCapabilities(config.videoCapabilities());

    DCHECK(config.hasDistinctiveIdentifier());
    web_config.distinctive_identifier =
        ConvertMediaKeysRequirement(config.distinctiveIdentifier());

    DCHECK(config.hasPersistentState());
    web_config.persistent_state =
        ConvertMediaKeysRequirement(config.persistentState());

    if (config.hasSessionTypes()) {
      web_config.session_types = ConvertSessionTypes(config.sessionTypes());
    } else {
      // From the spec
      // (http://w3c.github.io/encrypted-media/#idl-def-mediakeysystemconfiguration):
      // If this member is not present when the dictionary is passed to
      // requestMediaKeySystemAccess(), the dictionary will be treated
      // as if this member is set to [ "temporary" ].
      WebVector<WebEncryptedMediaSessionType> session_types(
          static_cast<size_t>(1));
      session_types[0] = WebEncryptedMediaSessionType::kTemporary;
      web_config.session_types = session_types;
    }

    // If |label| is not present, it will be a null string.
    web_config.label = config.label();
    supported_configurations_[i] = web_config;
  }

  CheckVideoCapabilityRobustness();
}

SecurityOrigin* MediaKeySystemAccessInitializer::GetSecurityOrigin() const {
  return IsExecutionContextValid()
             ? resolver_->GetExecutionContext()->GetSecurityOrigin()
             : nullptr;
}

void MediaKeySystemAccessInitializer::RequestSucceeded(
    WebContentDecryptionModuleAccess* access) {
  if (!IsExecutionContextValid())
    return;

  resolver_->Resolve(
      new MediaKeySystemAccess(key_system_, WTF::WrapUnique(access)));
  resolver_.Clear();
}

void MediaKeySystemAccessInitializer::RequestNotSupported(
    const WebString& error_message) {
  if (!IsExecutionContextValid())
    return;

  resolver_->Reject(DOMException::Create(kNotSupportedError, error_message));
  resolver_.Clear();
}

bool MediaKeySystemAccessInitializer::IsExecutionContextValid() const {
  // isContextDestroyed() is called to see if the context is in the
  // process of being destroyed. If it is true, assume the context is no
  // longer valid as it is about to be destroyed anyway.
  ExecutionContext* context = resolver_->GetExecutionContext();
  return context && !context->IsContextDestroyed();
}

void MediaKeySystemAccessInitializer::CheckVideoCapabilityRobustness() const {
  // Only check for widevine key system.
  if (KeySystem() != "com.widevine.alpha")
    return;

  bool has_video_capabilities = false;
  bool has_empty_robustness = false;

  for (const auto& config : supported_configurations_) {
    for (const auto& capability : config.video_capabilities) {
      has_video_capabilities = true;
      if (capability.robustness.IsEmpty()) {
        has_empty_robustness = true;
        break;
      }
    }

    if (has_empty_robustness)
      break;
  }

  if (has_video_capabilities) {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        EnumerationHistogram, empty_robustness_histogram,
        ("Media.EME.Widevine.VideoCapability.HasEmptyRobustness", 2));
    empty_robustness_histogram.Count(has_empty_robustness);
  }

  if (has_empty_robustness) {
    // TODO(xhwang): Write a best practice doc explaining details about risks of
    // using an empty robustness here, and provide the link to the doc in this
    // message. See http://crbug.com/720013
    resolver_->GetExecutionContext()->AddConsoleMessage(ConsoleMessage::Create(
        kJSMessageSource, kWarningMessageLevel,
        "It is recommended that a robustness level be specified. Not "
        "specifying the robustness level could result in unexpected "
        "behavior."));
  }
}

}  // namespace

ScriptPromise NavigatorRequestMediaKeySystemAccess::requestMediaKeySystemAccess(
    ScriptState* script_state,
    Navigator& navigator,
    const String& key_system,
    const HeapVector<MediaKeySystemConfiguration>& supported_configurations) {
  DVLOG(3) << __func__;

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  Document* document = ToDocument(execution_context);

  Deprecation::CountDeprecationFeaturePolicy(
      *document, WebFeaturePolicyFeature::kEncryptedMedia);

  if (RuntimeEnabledFeatures::FeaturePolicyForEncryptedMediaEnabled()) {
    if (!document->GetFrame() ||
        !document->GetFrame()->IsFeatureEnabled(
            WebFeaturePolicyFeature::kEncryptedMedia)) {
      return ScriptPromise::RejectWithDOMException(
          script_state,
          DOMException::Create(
              kNotSupportedError,
              "requestMediaKeySystemAccess is disabled by feature policy."));
    }
  }

  // From https://w3c.github.io/encrypted-media/#common-key-systems
  // All user agents MUST support the common key systems described in this
  // section.
  // 9.1 Clear Key: The "org.w3.clearkey" Key System uses plain-text clear
  //                (unencrypted) key(s) to decrypt the source.
  //
  // Do not check settings for Clear Key.
  if (key_system != "org.w3.clearkey") {
    // For other key systems, check settings and report UMA.
    bool encypted_media_enabled =
        document->GetSettings() &&
        document->GetSettings()->GetEncryptedMediaEnabled();

    static bool has_reported_uma = false;
    if (!has_reported_uma) {
      has_reported_uma = true;
      DEFINE_STATIC_LOCAL(BooleanHistogram, histogram,
                          ("Media.EME.EncryptedMediaEnabled"));
      histogram.Count(encypted_media_enabled);
    }

    if (!encypted_media_enabled) {
      return ScriptPromise::RejectWithDOMException(
          script_state,
          DOMException::Create(kNotSupportedError, "Unsupported keySystem"));
    }
  }

  // From https://w3c.github.io/encrypted-media/#requestMediaKeySystemAccess
  // When this method is invoked, the user agent must run the following steps:
  // 1. If keySystem is the empty string, return a promise rejected with a
  //    newly created TypeError.
  if (key_system.IsEmpty()) {
    return ScriptPromise::Reject(
        script_state,
        V8ThrowException::CreateTypeError(script_state->GetIsolate(),
                                          "The keySystem parameter is empty."));
  }

  // 2. If supportedConfigurations is empty, return a promise rejected with
  //    a newly created TypeError.
  if (!supported_configurations.size()) {
    return ScriptPromise::Reject(
        script_state, V8ThrowException::CreateTypeError(
                          script_state->GetIsolate(),
                          "The supportedConfigurations parameter is empty."));
  }

  // 3. Let document be the calling context's Document.
  //    (Done at the begining of this function.)
  if (!document->GetPage()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(
            kInvalidStateError,
            "The context provided is not associated with a page."));
  }

  UseCounter::Count(*document, WebFeature::kEncryptedMediaSecureOrigin);
  UseCounter::CountCrossOriginIframe(
      *document, WebFeature::kEncryptedMediaCrossOriginIframe);

  // 4. Let origin be the origin of document.
  //    (Passed with the execution context.)

  // 5. Let promise be a new promise.
  MediaKeySystemAccessInitializer* initializer =
      new MediaKeySystemAccessInitializer(script_state, key_system,
                                          supported_configurations);
  ScriptPromise promise = initializer->Promise();

  // 6. Asynchronously determine support, and if allowed, create and
  //    initialize the MediaKeySystemAccess object.
  MediaKeysController* controller =
      MediaKeysController::From(document->GetPage());
  WebEncryptedMediaClient* media_client =
      controller->EncryptedMediaClient(execution_context);
  media_client->RequestMediaKeySystemAccess(
      WebEncryptedMediaRequest(initializer));

  // 7. Return promise.
  return promise;
}

}  // namespace blink
