// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentInstruments.h"

#include <utility>

#include "base/location.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/frame/Frame.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/ConsoleMessage.h"
#include "modules/payments/BasicCardHelper.h"
#include "modules/payments/PaymentInstrument.h"
#include "modules/payments/PaymentManager.h"
#include "modules/permissions/PermissionUtils.h"
#include "platform/wtf/Vector.h"
#include "public/platform/TaskType.h"
#include "public/platform/WebIconSizesParser.h"
#include "public/platform/modules/manifest/manifest.mojom-blink.h"
#include "public/platform/web_feature.mojom-blink.h"

namespace blink {
namespace {

static const char kPaymentManagerUnavailable[] = "Payment manager unavailable";

bool rejectError(ScriptPromiseResolver* resolver,
                 payments::mojom::blink::PaymentHandlerStatus status) {
  switch (status) {
    case payments::mojom::blink::PaymentHandlerStatus::SUCCESS:
      return false;
    case payments::mojom::blink::PaymentHandlerStatus::NOT_IMPLEMENTED:
      resolver->Reject(
          DOMException::Create(kNotSupportedError, "Not implemented yet"));
      return true;
    case payments::mojom::blink::PaymentHandlerStatus::NOT_FOUND:
      resolver->Resolve();
      return true;
    case payments::mojom::blink::PaymentHandlerStatus::NO_ACTIVE_WORKER:
      resolver->Reject(
          DOMException::Create(kInvalidStateError, "No active service worker"));
      return true;
    case payments::mojom::blink::PaymentHandlerStatus::STORAGE_OPERATION_FAILED:
      resolver->Reject(DOMException::Create(kInvalidStateError,
                                            "Storage operation is failed"));
      return true;
    case payments::mojom::blink::PaymentHandlerStatus::
        FETCH_INSTRUMENT_ICON_FAILED: {
      ScriptState::Scope scope(resolver->GetScriptState());
      resolver->Reject(V8ThrowException::CreateTypeError(
          resolver->GetScriptState()->GetIsolate(),
          "Fetch or decode instrument icon failed"));
      return true;
    }
    case payments::mojom::blink::PaymentHandlerStatus::
        FETCH_PAYMENT_APP_INFO_FAILED:
      // FETCH_PAYMENT_APP_INFO_FAILED indicates everything works well except
      // fetching payment handler's name and/or icon from its web app manifest.
      // The origin or name will be used to label this payment handler in
      // UI in this case, so only show warnning message instead of reject the
      // promise. The warning message was printed by
      // payment_app_info_fetcher.cc.
      return false;
  }
  NOTREACHED();
  return false;
}

}  // namespace

PaymentInstruments::PaymentInstruments(
    const payments::mojom::blink::PaymentManagerPtr& manager)
    : manager_(manager) {}

ScriptPromise PaymentInstruments::deleteInstrument(
    ScriptState* script_state,
    const String& instrument_key) {
  if (!manager_.is_bound()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(kInvalidStateError, kPaymentManagerUnavailable));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  manager_->DeletePaymentInstrument(
      instrument_key,
      WTF::Bind(&PaymentInstruments::onDeletePaymentInstrument,
                WrapPersistent(this), WrapPersistent(resolver)));
  return promise;
}

ScriptPromise PaymentInstruments::get(ScriptState* script_state,
                                      const String& instrument_key) {
  if (!manager_.is_bound()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(kInvalidStateError, kPaymentManagerUnavailable));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  manager_->GetPaymentInstrument(
      instrument_key,
      WTF::Bind(&PaymentInstruments::onGetPaymentInstrument,
                WrapPersistent(this), WrapPersistent(resolver)));
  return promise;
}

ScriptPromise PaymentInstruments::keys(ScriptState* script_state) {
  if (!manager_.is_bound()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(kInvalidStateError, kPaymentManagerUnavailable));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  manager_->KeysOfPaymentInstruments(
      WTF::Bind(&PaymentInstruments::onKeysOfPaymentInstruments,
                WrapPersistent(this), WrapPersistent(resolver)));
  return promise;
}

ScriptPromise PaymentInstruments::has(ScriptState* script_state,
                                      const String& instrument_key) {
  if (!manager_.is_bound()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(kInvalidStateError, kPaymentManagerUnavailable));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  manager_->HasPaymentInstrument(
      instrument_key,
      WTF::Bind(&PaymentInstruments::onHasPaymentInstrument,
                WrapPersistent(this), WrapPersistent(resolver)));
  return promise;
}

ScriptPromise PaymentInstruments::set(ScriptState* script_state,
                                      const String& instrument_key,
                                      const PaymentInstrument& details,
                                      ExceptionState& exception_state) {
  if (!manager_.is_bound()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(kInvalidStateError, kPaymentManagerUnavailable));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ExecutionContext* context = ExecutionContext::From(script_state);
  Document* doc = ToDocumentOrNull(context);

  // Should move this permission check to browser process.
  // Please see http://crbug.com/795929
  GetPermissionService(script_state)
      ->RequestPermission(
          CreatePermissionDescriptor(
              mojom::blink::PermissionName::PAYMENT_HANDLER),
          Frame::HasTransientUserActivation(doc ? doc->GetFrame() : nullptr),
          WTF::Bind(&PaymentInstruments::OnRequestPermission,
                    WrapPersistent(this), WrapPersistent(resolver),
                    instrument_key, details));
  return resolver->Promise();
}

ScriptPromise PaymentInstruments::clear(ScriptState* script_state) {
  if (!manager_.is_bound()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(kInvalidStateError, kPaymentManagerUnavailable));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  manager_->ClearPaymentInstruments(
      WTF::Bind(&PaymentInstruments::onClearPaymentInstruments,
                WrapPersistent(this), WrapPersistent(resolver)));
  return promise;
}

mojom::blink::PermissionService* PaymentInstruments::GetPermissionService(
    ScriptState* script_state) {
  if (!permission_service_) {
    ConnectToPermissionService(ExecutionContext::From(script_state),
                               mojo::MakeRequest(&permission_service_));
  }
  return permission_service_.get();
}

void PaymentInstruments::OnRequestPermission(
    ScriptPromiseResolver* resolver,
    const String& instrument_key,
    const PaymentInstrument& details,
    mojom::blink::PermissionStatus status) {
  DCHECK(resolver);
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed())
    return;

  ScriptState::Scope scope(resolver->GetScriptState());

  if (status != mojom::blink::PermissionStatus::GRANTED) {
    resolver->Reject(DOMException::Create(
        kNotAllowedError, "Not allowed to install this payment handler"));
    return;
  }

  payments::mojom::blink::PaymentInstrumentPtr instrument =
      payments::mojom::blink::PaymentInstrument::New();
  instrument->name = details.hasName() ? details.name() : WTF::g_empty_string;
  if (details.hasIcons()) {
    ExecutionContext* context =
        ExecutionContext::From(resolver->GetScriptState());
    for (const ImageObject image_object : details.icons()) {
      KURL parsed_url = context->CompleteURL(image_object.src());
      if (!parsed_url.IsValid() || !parsed_url.ProtocolIsInHTTPFamily()) {
        resolver->Reject(V8ThrowException::CreateTypeError(
            resolver->GetScriptState()->GetIsolate(),
            "'" + image_object.src() + "' is not a valid URL."));
        return;
      }

      mojom::blink::ManifestIconPtr icon = mojom::blink::ManifestIcon::New();
      icon->src = parsed_url;
      icon->type = image_object.type();
      icon->purpose.push_back(blink::mojom::ManifestIcon_Purpose::ANY);
      WebVector<WebSize> web_sizes =
          WebIconSizesParser::ParseIconSizes(image_object.sizes());
      for (const auto& web_size : web_sizes) {
        icon->sizes.push_back(web_size);
      }
      instrument->icons.push_back(std::move(icon));
    }
  }

  if (details.hasEnabledMethods()) {
    instrument->enabled_methods = details.enabledMethods();
  }

  if (details.hasCapabilities()) {
    v8::Local<v8::String> value;
    if (!v8::JSON::Stringify(resolver->GetScriptState()->GetContext(),
                             details.capabilities().V8Value().As<v8::Object>())
             .ToLocal(&value)) {
      resolver->Reject(V8ThrowException::CreateTypeError(
          resolver->GetScriptState()->GetIsolate(),
          "Capabilities should be a JSON-serializable object"));
      return;
    }
    instrument->stringified_capabilities = ToCoreString(value);
    if (instrument->enabled_methods.Contains("basic-card")) {
      ExceptionState exception_state(resolver->GetScriptState()->GetIsolate(),
                                     ExceptionState::kSetterContext,
                                     "PaymentInstruments", "set");
      BasicCardHelper::ParseBasiccardData(
          details.capabilities(), instrument->supported_networks,
          instrument->supported_types, exception_state);
      if (exception_state.HadException()) {
        exception_state.Reject(resolver);
        return;
      }
    }
  } else {
    instrument->stringified_capabilities = WTF::g_empty_string;
  }

  UseCounter::Count(resolver->GetExecutionContext(),
                    WebFeature::kPaymentHandler);

  manager_->SetPaymentInstrument(
      instrument_key, std::move(instrument),
      WTF::Bind(&PaymentInstruments::onSetPaymentInstrument,
                WrapPersistent(this), WrapPersistent(resolver)));
}

void PaymentInstruments::onDeletePaymentInstrument(
    ScriptPromiseResolver* resolver,
    payments::mojom::blink::PaymentHandlerStatus status) {
  DCHECK(resolver);
  resolver->Resolve(status ==
                    payments::mojom::blink::PaymentHandlerStatus::SUCCESS);
}

void PaymentInstruments::onGetPaymentInstrument(
    ScriptPromiseResolver* resolver,
    payments::mojom::blink::PaymentInstrumentPtr stored_instrument,
    payments::mojom::blink::PaymentHandlerStatus status) {
  DCHECK(resolver);
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed())
    return;

  ScriptState::Scope scope(resolver->GetScriptState());

  if (rejectError(resolver, status))
    return;
  PaymentInstrument instrument;
  instrument.setName(stored_instrument->name);

  HeapVector<ImageObject> icons;
  for (const auto& icon : stored_instrument->icons) {
    ImageObject image_object;
    image_object.setSrc(icon->src.GetString());
    image_object.setType(icon->type);
    String sizes = WTF::g_empty_string;
    for (const auto& size : icon->sizes) {
      sizes = sizes + String::Format("%dx%d ", size.width, size.height);
    }
    image_object.setSizes(sizes.StripWhiteSpace());
    icons.emplace_back(image_object);
  }
  instrument.setIcons(icons);

  Vector<String> enabled_methods;
  for (const auto& method : stored_instrument->enabled_methods) {
    enabled_methods.push_back(method);
  }

  instrument.setEnabledMethods(enabled_methods);
  if (!stored_instrument->stringified_capabilities.IsEmpty()) {
    ExceptionState exception_state(resolver->GetScriptState()->GetIsolate(),
                                   ExceptionState::kGetterContext,
                                   "PaymentInstruments", "get");
    instrument.setCapabilities(
        ScriptValue(resolver->GetScriptState(),
                    FromJSONString(resolver->GetScriptState()->GetIsolate(),
                                   resolver->GetScriptState()->GetContext(),
                                   stored_instrument->stringified_capabilities,
                                   exception_state)));
    if (exception_state.HadException()) {
      exception_state.Reject(resolver);
      return;
    }
  }
  resolver->Resolve(instrument);
}

void PaymentInstruments::onKeysOfPaymentInstruments(
    ScriptPromiseResolver* resolver,
    const Vector<String>& keys,
    payments::mojom::blink::PaymentHandlerStatus status) {
  DCHECK(resolver);
  if (rejectError(resolver, status))
    return;
  resolver->Resolve(keys);
}

void PaymentInstruments::onHasPaymentInstrument(
    ScriptPromiseResolver* resolver,
    payments::mojom::blink::PaymentHandlerStatus status) {
  DCHECK(resolver);
  resolver->Resolve(status ==
                    payments::mojom::blink::PaymentHandlerStatus::SUCCESS);
}

void PaymentInstruments::onSetPaymentInstrument(
    ScriptPromiseResolver* resolver,
    payments::mojom::blink::PaymentHandlerStatus status) {
  DCHECK(resolver);
  if (rejectError(resolver, status))
    return;
  resolver->Resolve();
}

void PaymentInstruments::onClearPaymentInstruments(
    ScriptPromiseResolver* resolver,
    payments::mojom::blink::PaymentHandlerStatus status) {
  DCHECK(resolver);
  if (rejectError(resolver, status))
    return;
  resolver->Resolve();
}

}  // namespace blink
