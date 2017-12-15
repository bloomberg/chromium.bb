// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/cookie_store/CookieStore.h"

#include <utility>

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "modules/cookie_store/CookieListItem.h"
#include "modules/cookie_store/CookieStoreGetOptions.h"
#include "modules/cookie_store/CookieStoreSetOptions.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScope.h"
#include "platform/bindings/ScriptState.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/Optional.h"
#include "platform/wtf/Time.h"
#include "platform/wtf/text/WTFString.h"
#include "services/network/public/interfaces/restricted_cookie_manager.mojom-blink.h"

namespace blink {

namespace {

// Returns null if and only if an exception is thrown.
network::mojom::blink::CookieManagerGetOptionsPtr ToBackendOptions(
    const String& name,  // Value of the "name" positional argument.
    const CookieStoreGetOptions& options,
    ExceptionState& exception_state) {
  auto backend_options = network::mojom::blink::CookieManagerGetOptions::New();

  // TODO(crbug.com/729800): Handle the url option.

  if (options.matchType() == "startsWith") {
    backend_options->match_type =
        network::mojom::blink::CookieMatchType::STARTS_WITH;
  } else {
    DCHECK_EQ(options.matchType(), WTF::String("equals"));
    backend_options->match_type =
        network::mojom::blink::CookieMatchType::EQUALS;
  }

  if (name.IsNull()) {
    if (options.hasName()) {
      backend_options->name = options.name();
    } else {
      // No name provided. Use a filter that matches all cookies. This overrides
      // a user-provided matchType.
      backend_options->match_type =
          network::mojom::blink::CookieMatchType::STARTS_WITH;
      backend_options->name = g_empty_string;
    }
  } else {
    if (options.hasName()) {
      exception_state.ThrowTypeError(
          "Cookie name specified both as an argument and as an option");
      return nullptr;
    }
    backend_options->name = name;
  }

  return backend_options;
}

// Returns null if and only if an exception is thrown.
network::mojom::blink::CanonicalCookiePtr ToCanonicalCookie(
    const KURL& cookie_url,
    const String& name,   // Value of the "name" positional argument.
    const String& value,  // Value of the "value" positional argument.
    bool for_deletion,    // True for CookieStore.delete, false for set.
    const CookieStoreSetOptions& options,
    ExceptionState& exception_state) {
  auto canonical_cookie = network::mojom::blink::CanonicalCookie::New();

  if (name.IsNull()) {
    if (!options.hasName()) {
      exception_state.ThrowTypeError("Unspecified cookie name");
      return nullptr;
    }
    canonical_cookie->name = options.name();
  } else {
    if (options.hasName()) {
      exception_state.ThrowTypeError(
          "Cookie name specified both as an argument and as an option");
      return nullptr;
    }
    canonical_cookie->name = name;
  }

  if (for_deletion) {
    DCHECK(value.IsNull());
    if (options.hasValue()) {
      exception_state.ThrowTypeError(
          "Cookie value is meaningless when deleting");
      return nullptr;
    }
    canonical_cookie->value = g_empty_string;

    if (options.hasExpires()) {
      exception_state.ThrowTypeError(
          "Cookie expiration time is meaningless when deleting");
      return nullptr;
    }
    canonical_cookie->expiry = WTF::Time::Min();
  } else {
    if (value.IsNull()) {
      if (!options.hasValue()) {
        exception_state.ThrowTypeError("Unspecified cookie value");
        return nullptr;
      }
      canonical_cookie->value = options.value();
    } else {
      if (options.hasValue()) {
        exception_state.ThrowTypeError(
            "Cookie value specified both as an argument and as an option");
        return nullptr;
      }
      canonical_cookie->value = value;
    }

    if (options.hasExpires())
      canonical_cookie->expiry = WTF::Time::FromJavaTime(options.expires());
    // The expires option is not set in CookieStoreSetOptions for session
    // cookies. This is represented by a null expiry field in CanonicalCookie.
  }

  if (options.hasDomain()) {
    // TODO(crbug.com/729800): Checks and exception throwing.
    canonical_cookie->domain = options.domain();
  } else {
    // TODO(crbug.com/729800): Correct value?
    canonical_cookie->domain = cookie_url.Host();
  }

  if (options.hasPath()) {
    canonical_cookie->path = options.path();
  } else {
    canonical_cookie->path = String("/");
  }

  bool is_secure_origin = SecurityOrigin::IsSecure(cookie_url);
  if (options.hasSecure()) {
    canonical_cookie->secure = options.secure();
  } else {
    canonical_cookie->secure = is_secure_origin;
  }

  if (name.StartsWith("__Secure-") || name.StartsWith("__Host-")) {
    if (!canonical_cookie->secure) {
      exception_state.ThrowTypeError(
          "__Secure- and __Host- cookies must be secure");
      return nullptr;
    }
    if (!is_secure_origin) {
      exception_state.ThrowTypeError(
          "__Secure- and __Host- cookies must be written from secure origin");
      return nullptr;
    }
  }

  canonical_cookie->httponly = options.httpOnly();
  return canonical_cookie;
}

// Computes the cookie origin and site URLs for a ScriptState.
void ExtractCookieURLs(ScriptState* script_state,
                       KURL& cookie_url,
                       KURL& site_for_cookies) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  if (execution_context->IsDocument()) {
    Document* document = ToDocument(execution_context);
    cookie_url = document->CookieURL();
    site_for_cookies = document->SiteForCookies();
  } else if (execution_context->IsServiceWorkerGlobalScope()) {
    ServiceWorkerGlobalScope* scope =
        ToServiceWorkerGlobalScope(execution_context);
    // TODO(crbug.com/729800): Correct values?
    cookie_url = scope->Url();
    site_for_cookies = scope->Url();
  } else {
    NOTIMPLEMENTED();
  }
}

}  // anonymous namespace

CookieStore::~CookieStore() = default;

CookieStore* CookieStore::Create(
    ExecutionContext* execution_context,
    network::mojom::blink::RestrictedCookieManagerPtr backend) {
  return new CookieStore(execution_context, std::move(backend));
}

ScriptPromise CookieStore::getAll(ScriptState* script_state,
                                  const CookieStoreGetOptions& options,
                                  ExceptionState& exception_state) {
  return getAll(script_state, WTF::String(), options, exception_state);
}

ScriptPromise CookieStore::getAll(ScriptState* script_state,
                                  const String& name,
                                  const CookieStoreGetOptions& options,
                                  ExceptionState& exception_state) {
  return DoRead(script_state, name, options,
                &CookieStore::GetAllForUrlToGetAllResult, exception_state);
}

ScriptPromise CookieStore::get(ScriptState* script_state,
                               const CookieStoreGetOptions& options,
                               ExceptionState& exception_state) {
  return get(script_state, WTF::String(), options, exception_state);
}

ScriptPromise CookieStore::get(ScriptState* script_state,
                               const String& name,
                               const CookieStoreGetOptions& options,
                               ExceptionState& exception_state) {
  return DoRead(script_state, name, options,
                &CookieStore::GetAllForUrlToGetResult, exception_state);
}

ScriptPromise CookieStore::has(ScriptState* script_state,
                               const CookieStoreGetOptions& options,
                               ExceptionState& exception_state) {
  return has(script_state, WTF::String(), options, exception_state);
}

ScriptPromise CookieStore::has(ScriptState* script_state,
                               const String& name,
                               const CookieStoreGetOptions& options,
                               ExceptionState& exception_state) {
  return DoRead(script_state, name, options,
                &CookieStore::GetAllForUrlToHasResult, exception_state);
}

ScriptPromise CookieStore::set(ScriptState* script_state,
                               const CookieStoreSetOptions& options,
                               ExceptionState& exception_state) {
  return set(script_state, WTF::String(), WTF::String(), options,
             exception_state);
}

ScriptPromise CookieStore::set(ScriptState* script_state,
                               const String& name,
                               const String& value,
                               const CookieStoreSetOptions& options,
                               ExceptionState& exception_state) {
  return DoWrite(script_state, name, value, options, false /* is_deletion */,
                 exception_state);
}

ScriptPromise CookieStore::Delete(ScriptState* script_state,
                                  const CookieStoreSetOptions& options,
                                  ExceptionState& exception_state) {
  return Delete(script_state, WTF::String(), options, exception_state);
}

ScriptPromise CookieStore::Delete(ScriptState* script_state,
                                  const String& name,
                                  const CookieStoreSetOptions& options,
                                  ExceptionState& exception_state) {
  return DoWrite(script_state, name, WTF::String(), options,
                 true /* is_deletion */, exception_state);
}

void CookieStore::ContextDestroyed(ExecutionContext* execution_context) {
  backend_.reset();
}

CookieStore::CookieStore(
    ExecutionContext* execution_context,
    network::mojom::blink::RestrictedCookieManagerPtr backend)
    : ContextLifecycleObserver(execution_context),
      backend_(std::move(backend)) {
  DCHECK(backend_);
}

ScriptPromise CookieStore::DoRead(
    ScriptState* script_state,
    const String& name,
    const CookieStoreGetOptions& options,
    DoReadBackendResultConverter backend_result_converter,
    ExceptionState& exception_state) {
  KURL cookie_url;
  KURL site_for_cookies;
  ExtractCookieURLs(script_state, cookie_url, site_for_cookies);

  network::mojom::blink::CookieManagerGetOptionsPtr backend_options =
      ToBackendOptions(name, options, exception_state);
  if (backend_options.is_null())
    return ScriptPromise();  // ToBackendOptions has thrown an exception.

  if (!backend_) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "CookieStore backend went away");
    return ScriptPromise();
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  backend_->GetAllForUrl(
      cookie_url, site_for_cookies, std::move(backend_options),
      WTF::Bind(backend_result_converter, WrapPersistent(resolver)));
  return resolver->Promise();
}

// static
void CookieStore::GetAllForUrlToGetAllResult(
    ScriptPromiseResolver* resolver,
    Vector<network::mojom::blink::CanonicalCookiePtr> backend_cookies) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;

  HeapVector<CookieListItem> cookies;
  cookies.ReserveInitialCapacity(backend_cookies.size());
  for (const auto& canonical_cookie : backend_cookies) {
    CookieListItem& cookie = cookies.emplace_back();
    cookie.setName(canonical_cookie->name);
    cookie.setValue(canonical_cookie->value);
  }

  resolver->Resolve(cookies);
}

// static
void CookieStore::GetAllForUrlToGetResult(
    ScriptPromiseResolver* resolver,
    Vector<network::mojom::blink::CanonicalCookiePtr> backend_cookies) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;

  if (backend_cookies.IsEmpty()) {
    resolver->Resolve(v8::Null(script_state->GetIsolate()));
    return;
  }

  const auto& canonical_cookie = backend_cookies.front();
  CookieListItem cookie;
  cookie.setName(canonical_cookie->name);
  cookie.setValue(canonical_cookie->value);
  resolver->Resolve(cookie);
}

// static
void CookieStore::GetAllForUrlToHasResult(
    ScriptPromiseResolver* resolver,
    Vector<network::mojom::blink::CanonicalCookiePtr> backend_cookies) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;

  resolver->Resolve(!backend_cookies.IsEmpty());
}

ScriptPromise CookieStore::DoWrite(ScriptState* script_state,
                                   const String& name,
                                   const String& value,
                                   const CookieStoreSetOptions& options,
                                   bool is_deletion,
                                   ExceptionState& exception_state) {
  KURL cookie_url;
  KURL site_for_cookies;
  ExtractCookieURLs(script_state, cookie_url, site_for_cookies);

  network::mojom::blink::CanonicalCookiePtr canonical_cookie =
      ToCanonicalCookie(cookie_url, name, value, is_deletion, options,
                        exception_state);
  if (canonical_cookie.is_null())
    return ScriptPromise();  // ToCanonicalCookie has thrown an exception.

  if (!backend_) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "CookieStore backend went away");
    return ScriptPromise();
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  backend_->SetCanonicalCookie(
      std::move(canonical_cookie), cookie_url, site_for_cookies,
      WTF::Bind(&CookieStore::OnSetCanonicalCookieResult,
                WrapPersistent(resolver)));
  return resolver->Promise();
}

// static
void CookieStore::OnSetCanonicalCookieResult(ScriptPromiseResolver* resolver,
                                             bool backend_success) {
  ScriptState* script_state = resolver->GetScriptState();
  if (!script_state->ContextIsValid())
    return;

  if (!backend_success) {
    resolver->Reject(DOMException::Create(
        kUnknownError, "An unknown error occured while writing the cookie."));
    return;
  }
  resolver->Resolve();
}

}  // namespace blink
