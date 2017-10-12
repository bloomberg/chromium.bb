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
#include "platform/wtf/Functional.h"
#include "platform/wtf/Optional.h"
#include "platform/wtf/Time.h"
#include "platform/wtf/text/WTFString.h"
#include "services/network/public/interfaces/restricted_cookie_manager.mojom-blink.h"

namespace blink {

namespace {

network::mojom::blink::CookieManagerGetOptionsPtr ToBackendOptions(
    const CookieStoreGetOptions& options) {
  auto backend_options = network::mojom::blink::CookieManagerGetOptions::New();
  backend_options->name = g_empty_string;
  backend_options->match_type =
      network::mojom::blink::CookieMatchType::STARTS_WITH;

  // TODO(crbug.com/729800): Implement, parse and validate options. Throw
  //                         relevant exceptions on validation errors.
  return backend_options;
}

network::mojom::blink::CanonicalCookiePtr ToCanonicalCookie(
    const KURL& cookie_url,
    const String& name,
    const String& value,
    const CookieStoreSetOptions& options) {
  auto canonical_cookie = network::mojom::blink::CanonicalCookie::New();

  DCHECK(!name.IsNull());
  canonical_cookie->name = name;

  DCHECK(!value.IsNull());
  canonical_cookie->value = value;

  // TODO(pwnall): What if expires isn't set?
  if (options.hasExpires())
    canonical_cookie->expiry = WTF::Time::FromJavaTime(options.expires());

  // TODO(pwnall): Find correct value.
  canonical_cookie->domain = cookie_url.Host();
  canonical_cookie->path = String("/");
  canonical_cookie->secure = false;
  canonical_cookie->httponly = false;

  // TODO(crbug.com/729800): Implement, parse and validate options. Throw
  //                         relevant exceptions on validation errors.
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
  } else {
    // TODO(crbug.com/729800): Add branch for service workers.
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
  KURL cookie_url;
  KURL site_for_cookies;
  ExtractCookieURLs(script_state, cookie_url, site_for_cookies);

  network::mojom::blink::CookieManagerGetOptionsPtr backend_options =
      ToBackendOptions(options);
  DCHECK(!backend_options.is_null());

  if (!backend_) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "CookieStore backend went away");
    return ScriptPromise();
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  backend_->GetAllForUrl(
      cookie_url, site_for_cookies, std::move(backend_options),
      ConvertToBaseCallback(WTF::Bind(&CookieStore::OnGetAllForUrlResult,
                                      WrapPersistent(this),
                                      WrapPersistent(resolver))));
  return resolver->Promise();
}

ScriptPromise CookieStore::set(ScriptState* script_state,
                               const String& name,
                               const String& value,
                               const CookieStoreSetOptions& options,
                               ExceptionState& exception_state) {
  KURL cookie_url;
  KURL site_for_cookies;
  ExtractCookieURLs(script_state, cookie_url, site_for_cookies);

  network::mojom::blink::CanonicalCookiePtr canonical_cookie =
      ToCanonicalCookie(cookie_url, name, value, options);
  DCHECK(!canonical_cookie.is_null());

  if (!backend_) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "CookieStore backend went away");
    return ScriptPromise();
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  backend_->SetCanonicalCookie(
      std::move(canonical_cookie), cookie_url, site_for_cookies,
      ConvertToBaseCallback(WTF::Bind(&CookieStore::OnSetCanonicalCookieResult,
                                      WrapPersistent(this),
                                      WrapPersistent(resolver))));
  return resolver->Promise();
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

void CookieStore::OnGetAllForUrlResult(
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

void CookieStore::OnSetCanonicalCookieResult(ScriptPromiseResolver* resolver,
                                             bool backend_success) {
  if (!backend_success) {
    resolver->Reject(DOMException::Create(
        kUnknownError, "An unknown error occured while writing the cookie."));
    return;
  }
  resolver->Resolve();
}

}  // namespace blink
