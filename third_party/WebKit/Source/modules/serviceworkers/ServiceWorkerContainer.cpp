/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "modules/serviceworkers/ServiceWorkerContainer.h"

#include <memory>
#include <utility>
#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/serialization/SerializedScriptValue.h"
#include "bindings/core/v8/serialization/SerializedScriptValueFactory.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/MessageEvent.h"
#include "core/frame/Deprecation.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/UseCounter.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/messaging/MessagePort.h"
#include "modules/EventTargetModules.h"
#include "modules/serviceworkers/NavigatorServiceWorker.h"
#include "modules/serviceworkers/ServiceWorker.h"
#include "modules/serviceworkers/ServiceWorkerContainerClient.h"
#include "modules/serviceworkers/ServiceWorkerError.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"
#include "platform/bindings/ScriptState.h"
#include "platform/bindings/V8ThrowException.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityViolationReportingPolicy.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/modules/serviceworker/WebServiceWorker.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerProvider.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerRegistration.h"
#include "third_party/WebKit/common/service_worker/service_worker_error_type.mojom-blink.h"

namespace blink {

namespace {

mojom::ServiceWorkerUpdateViaCache ParseUpdateViaCache(const String& value) {
  if (value == "imports")
    return mojom::ServiceWorkerUpdateViaCache::kImports;
  if (value == "all")
    return mojom::ServiceWorkerUpdateViaCache::kAll;
  if (value == "none")
    return mojom::ServiceWorkerUpdateViaCache::kNone;
  // Default value.
  return mojom::ServiceWorkerUpdateViaCache::kImports;
}

class GetRegistrationCallback : public WebServiceWorkerProvider::
                                    WebServiceWorkerGetRegistrationCallbacks {
 public:
  explicit GetRegistrationCallback(ScriptPromiseResolver* resolver)
      : resolver_(resolver) {}
  ~GetRegistrationCallback() override = default;

  void OnSuccess(
      std::unique_ptr<WebServiceWorkerRegistration::Handle> handle) override {
    if (!resolver_->GetExecutionContext() ||
        resolver_->GetExecutionContext()->IsContextDestroyed())
      return;
    if (!handle) {
      // Resolve the promise with undefined.
      resolver_->Resolve();
      return;
    }
    resolver_->Resolve(ServiceWorkerRegistration::GetOrCreate(
        resolver_->GetExecutionContext(), std::move(handle)));
  }

  void OnError(const WebServiceWorkerError& error) override {
    if (!resolver_->GetExecutionContext() ||
        resolver_->GetExecutionContext()->IsContextDestroyed())
      return;
    resolver_->Reject(ServiceWorkerError::Take(resolver_.Get(), error));
  }

 private:
  Persistent<ScriptPromiseResolver> resolver_;
  WTF_MAKE_NONCOPYABLE(GetRegistrationCallback);
};

}  // namespace

class ServiceWorkerContainer::GetRegistrationForReadyCallback
    : public WebServiceWorkerProvider::
          WebServiceWorkerGetRegistrationForReadyCallbacks {
 public:
  explicit GetRegistrationForReadyCallback(ReadyProperty* ready)
      : ready_(ready) {}
  ~GetRegistrationForReadyCallback() override = default;

  void OnSuccess(
      std::unique_ptr<WebServiceWorkerRegistration::Handle> handle) override {
    DCHECK_EQ(ready_->GetState(), ReadyProperty::kPending);

    if (ready_->GetExecutionContext() &&
        !ready_->GetExecutionContext()->IsContextDestroyed()) {
      ready_->Resolve(ServiceWorkerRegistration::GetOrCreate(
          ready_->GetExecutionContext(), std::move(handle)));
    }
  }

 private:
  Persistent<ReadyProperty> ready_;
  WTF_MAKE_NONCOPYABLE(GetRegistrationForReadyCallback);
};

ServiceWorkerContainer* ServiceWorkerContainer::Create(
    ExecutionContext* execution_context,
    NavigatorServiceWorker* navigator) {
  return new ServiceWorkerContainer(execution_context, navigator);
}

ServiceWorkerContainer::~ServiceWorkerContainer() {
  DCHECK(!provider_);
}

void ServiceWorkerContainer::ContextDestroyed(ExecutionContext*) {
  if (provider_) {
    provider_->SetClient(nullptr);
    provider_ = nullptr;
  }
  controller_ = nullptr;
  navigator_->ClearServiceWorker();
}

void ServiceWorkerContainer::Trace(blink::Visitor* visitor) {
  visitor->Trace(controller_);
  visitor->Trace(ready_);
  visitor->Trace(navigator_);
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

void ServiceWorkerContainer::RegisterServiceWorkerImpl(
    ExecutionContext* execution_context,
    const KURL& raw_script_url,
    const KURL& scope,
    mojom::ServiceWorkerUpdateViaCache update_via_cache,
    std::unique_ptr<RegistrationCallbacks> callbacks) {
  if (!provider_) {
    callbacks->OnError(
        WebServiceWorkerError(mojom::blink::ServiceWorkerErrorType::kState,
                              "Failed to register a ServiceWorker: The "
                              "document is in an invalid state."));
    return;
  }

  scoped_refptr<const SecurityOrigin> document_origin =
      execution_context->GetSecurityOrigin();
  String error_message;
  // Restrict to secure origins:
  // https://w3c.github.io/webappsec/specs/powerfulfeatures/#settings-privileged
  if (!execution_context->IsSecureContext(error_message)) {
    callbacks->OnError(WebServiceWorkerError(
        mojom::blink::ServiceWorkerErrorType::kSecurity, error_message));
    return;
  }

  KURL page_url = KURL(NullURL(), document_origin->ToString());
  if (!SchemeRegistry::ShouldTreatURLSchemeAsAllowingServiceWorkers(
          page_url.Protocol())) {
    callbacks->OnError(WebServiceWorkerError(
        mojom::blink::ServiceWorkerErrorType::kSecurity,
        String("Failed to register a ServiceWorker: The URL protocol of the "
               "current origin ('" +
               document_origin->ToString() + "') is not supported.")));
    return;
  }

  KURL script_url = raw_script_url;
  script_url.RemoveFragmentIdentifier();
  if (!document_origin->CanRequest(script_url)) {
    scoped_refptr<const SecurityOrigin> script_origin =
        SecurityOrigin::Create(script_url);
    callbacks->OnError(
        WebServiceWorkerError(mojom::blink::ServiceWorkerErrorType::kSecurity,
                              String("Failed to register a ServiceWorker: The "
                                     "origin of the provided scriptURL ('" +
                                     script_origin->ToString() +
                                     "') does not match the current origin ('" +
                                     document_origin->ToString() + "').")));
    return;
  }
  if (!SchemeRegistry::ShouldTreatURLSchemeAsAllowingServiceWorkers(
          script_url.Protocol())) {
    callbacks->OnError(WebServiceWorkerError(
        mojom::blink::ServiceWorkerErrorType::kSecurity,
        String("Failed to register a ServiceWorker: The URL protocol of the "
               "script ('" +
               script_url.GetString() + "') is not supported.")));
    return;
  }

  KURL pattern_url = scope;
  pattern_url.RemoveFragmentIdentifier();

  if (!document_origin->CanRequest(pattern_url)) {
    scoped_refptr<const SecurityOrigin> pattern_origin =
        SecurityOrigin::Create(pattern_url);
    callbacks->OnError(
        WebServiceWorkerError(mojom::blink::ServiceWorkerErrorType::kSecurity,
                              String("Failed to register a ServiceWorker: The "
                                     "origin of the provided scope ('" +
                                     pattern_origin->ToString() +
                                     "') does not match the current origin ('" +
                                     document_origin->ToString() + "').")));
    return;
  }
  if (!SchemeRegistry::ShouldTreatURLSchemeAsAllowingServiceWorkers(
          pattern_url.Protocol())) {
    callbacks->OnError(WebServiceWorkerError(
        mojom::blink::ServiceWorkerErrorType::kSecurity,
        String("Failed to register a ServiceWorker: The URL protocol of the "
               "scope ('" +
               pattern_url.GetString() + "') is not supported.")));
    return;
  }

  WebString web_error_message;
  if (!provider_->ValidateScopeAndScriptURL(pattern_url, script_url,
                                            &web_error_message)) {
    callbacks->OnError(WebServiceWorkerError(
        mojom::blink::ServiceWorkerErrorType::kType,
        WebString::FromUTF8("Failed to register a ServiceWorker: " +
                            web_error_message.Utf8())));
    return;
  }

  ContentSecurityPolicy* csp = execution_context->GetContentSecurityPolicy();
  if (csp) {
    if (!(csp->AllowRequestWithoutIntegrity(
              WebURLRequest::kRequestContextServiceWorker, script_url) &&
          csp->AllowWorkerContextFromSource(
              script_url, ResourceRequest::RedirectStatus::kNoRedirect,
              SecurityViolationReportingPolicy::kReport))) {
      callbacks->OnError(WebServiceWorkerError(
          mojom::blink::ServiceWorkerErrorType::kSecurity,
          String(
              "Failed to register a ServiceWorker: The provided scriptURL ('" +
              script_url.GetString() +
              "') violates the Content Security Policy.")));
      return;
    }
  }

  provider_->RegisterServiceWorker(pattern_url, script_url, update_via_cache,
                                   std::move(callbacks));
}

ScriptPromise ServiceWorkerContainer::registerServiceWorker(
    ScriptState* script_state,
    const String& url,
    const RegistrationOptions& options) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!provider_) {
    resolver->Reject(DOMException::Create(kInvalidStateError,
                                          "Failed to register a ServiceWorker: "
                                          "The document is in an invalid "
                                          "state."));
    return promise;
  }

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  // FIXME: May be null due to worker termination: http://crbug.com/413518.
  if (!execution_context)
    return ScriptPromise();

  KURL script_url = execution_context->CompleteURL(url);
  script_url.RemoveFragmentIdentifier();

  KURL pattern_url;
  if (options.scope().IsNull())
    pattern_url = KURL(script_url, "./");
  else
    pattern_url = execution_context->CompleteURL(options.scope());

  mojom::ServiceWorkerUpdateViaCache update_via_cache =
      ParseUpdateViaCache(options.updateViaCache());

  RegisterServiceWorkerImpl(
      execution_context, script_url, pattern_url, update_via_cache,
      std::make_unique<CallbackPromiseAdapter<ServiceWorkerRegistration,
                                              ServiceWorkerErrorForUpdate>>(
          resolver));

  return promise;
}

ScriptPromise ServiceWorkerContainer::getRegistration(
    ScriptState* script_state,
    const String& document_url) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!provider_) {
    resolver->Reject(DOMException::Create(kInvalidStateError,
                                          "Failed to get a "
                                          "ServiceWorkerRegistration: The "
                                          "document is in an invalid state."));
    return promise;
  }

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  // FIXME: May be null due to worker termination: http://crbug.com/413518.
  if (!execution_context)
    return ScriptPromise();

  scoped_refptr<const SecurityOrigin> document_origin =
      execution_context->GetSecurityOrigin();
  String error_message;
  if (!execution_context->IsSecureContext(error_message)) {
    resolver->Reject(DOMException::Create(kSecurityError, error_message));
    return promise;
  }

  KURL page_url = KURL(NullURL(), document_origin->ToString());
  if (!SchemeRegistry::ShouldTreatURLSchemeAsAllowingServiceWorkers(
          page_url.Protocol())) {
    resolver->Reject(DOMException::Create(
        kSecurityError,
        "Failed to get a ServiceWorkerRegistration: The URL protocol of the "
        "current origin ('" +
            document_origin->ToString() + "') is not supported."));
    return promise;
  }

  KURL completed_url = execution_context->CompleteURL(document_url);
  completed_url.RemoveFragmentIdentifier();
  if (!document_origin->CanRequest(completed_url)) {
    scoped_refptr<const SecurityOrigin> document_url_origin =
        SecurityOrigin::Create(completed_url);
    resolver->Reject(
        DOMException::Create(kSecurityError,
                             "Failed to get a ServiceWorkerRegistration: The "
                             "origin of the provided documentURL ('" +
                                 document_url_origin->ToString() +
                                 "') does not match the current origin ('" +
                                 document_origin->ToString() + "')."));
    return promise;
  }
  provider_->GetRegistration(
      completed_url, std::make_unique<GetRegistrationCallback>(resolver));

  return promise;
}

ScriptPromise ServiceWorkerContainer::getRegistrations(
    ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!provider_) {
    resolver->Reject(
        DOMException::Create(kInvalidStateError,
                             "Failed to get ServiceWorkerRegistration objects: "
                             "The document is in an invalid state."));
    return promise;
  }

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  scoped_refptr<const SecurityOrigin> document_origin =
      execution_context->GetSecurityOrigin();
  String error_message;
  if (!execution_context->IsSecureContext(error_message)) {
    resolver->Reject(DOMException::Create(kSecurityError, error_message));
    return promise;
  }

  KURL page_url = KURL(NullURL(), document_origin->ToString());
  if (!SchemeRegistry::ShouldTreatURLSchemeAsAllowingServiceWorkers(
          page_url.Protocol())) {
    resolver->Reject(DOMException::Create(
        kSecurityError,
        "Failed to get ServiceWorkerRegistration objects: The URL protocol of "
        "the current origin ('" +
            document_origin->ToString() + "') is not supported."));
    return promise;
  }

  provider_->GetRegistrations(
      std::make_unique<CallbackPromiseAdapter<ServiceWorkerRegistrationArray,
                                              ServiceWorkerError>>(resolver));

  return promise;
}

ServiceWorkerContainer::ReadyProperty*
ServiceWorkerContainer::CreateReadyProperty() {
  return new ReadyProperty(GetExecutionContext(), this, ReadyProperty::kReady);
}

ScriptPromise ServiceWorkerContainer::ready(ScriptState* caller_state) {
  if (!GetExecutionContext())
    return ScriptPromise();

  if (!caller_state->World().IsMainWorld()) {
    // FIXME: Support .ready from isolated worlds when
    // ScriptPromiseProperty can vend Promises in isolated worlds.
    return ScriptPromise::RejectWithDOMException(
        caller_state,
        DOMException::Create(kNotSupportedError,
                             "'ready' is only supported in pages."));
  }

  if (!ready_) {
    ready_ = CreateReadyProperty();
    if (provider_) {
      provider_->GetRegistrationForReady(
          std::make_unique<GetRegistrationForReadyCallback>(ready_.Get()));
    }
  }

  return ready_->Promise(caller_state->World());
}

void ServiceWorkerContainer::SetController(
    std::unique_ptr<WebServiceWorker::Handle> handle,
    bool should_notify_controller_change) {
  if (!GetExecutionContext())
    return;
  controller_ = ServiceWorker::From(GetExecutionContext(), std::move(handle));
  if (controller_) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kServiceWorkerControlledPage);
  }
  if (should_notify_controller_change)
    DispatchEvent(Event::Create(EventTypeNames::controllerchange));
}

void ServiceWorkerContainer::DispatchMessageEvent(
    std::unique_ptr<WebServiceWorker::Handle> handle,
    const WebString& message,
    WebVector<MessagePortChannel> channels) {
  if (!GetExecutionContext() || !GetExecutionContext()->ExecutingWindow())
    return;

  MessagePortArray* ports =
      MessagePort::EntanglePorts(*GetExecutionContext(), std::move(channels));
  scoped_refptr<SerializedScriptValue> value =
      SerializedScriptValue::Create(message);
  ServiceWorker* source =
      ServiceWorker::From(GetExecutionContext(), std::move(handle));
  DispatchEvent(MessageEvent::Create(
      ports, value, GetExecutionContext()->GetSecurityOrigin()->ToString(),
      String() /* lastEventId */, source, String() /* suborigin */));
}

void ServiceWorkerContainer::CountFeature(mojom::WebFeature feature) {
  if (!GetExecutionContext())
    return;
  if (Deprecation::DeprecationMessage(feature).IsEmpty())
    UseCounter::Count(GetExecutionContext(), feature);
  else
    Deprecation::CountDeprecation(GetExecutionContext(), feature);
}

const AtomicString& ServiceWorkerContainer::InterfaceName() const {
  return EventTargetNames::ServiceWorkerContainer;
}

ServiceWorkerContainer::ServiceWorkerContainer(
    ExecutionContext* execution_context,
    NavigatorServiceWorker* navigator)
    : ContextLifecycleObserver(execution_context),
      provider_(nullptr),
      navigator_(navigator) {
  if (!execution_context)
    return;

  if (ServiceWorkerContainerClient* client =
          ServiceWorkerContainerClient::From(execution_context)) {
    provider_ = client->Provider();
    if (provider_)
      provider_->SetClient(this);
  }
}

}  // namespace blink
