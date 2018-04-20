// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cookie_manager.h"

#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_options.h"
#include "url/gurl.h"

using CookieDeletionInfo = net::CookieStore::CookieDeletionInfo;
using CookieDeleteSessionControl =
    net::CookieStore::CookieDeletionInfo::SessionControl;

namespace network {

namespace {

network::mojom::CookieChangeCause ChangeCauseTranslation(
    net::CookieChangeCause net_cause) {
  switch (net_cause) {
    case net::CookieChangeCause::INSERTED:
      return network::mojom::CookieChangeCause::INSERTED;
    case net::CookieChangeCause::EXPLICIT:
      return network::mojom::CookieChangeCause::EXPLICIT;
    case net::CookieChangeCause::UNKNOWN_DELETION:
      return network::mojom::CookieChangeCause::UNKNOWN_DELETION;
    case net::CookieChangeCause::OVERWRITE:
      return network::mojom::CookieChangeCause::OVERWRITE;
    case net::CookieChangeCause::EXPIRED:
      return network::mojom::CookieChangeCause::EXPIRED;
    case net::CookieChangeCause::EVICTED:
      return network::mojom::CookieChangeCause::EVICTED;
    case net::CookieChangeCause::EXPIRED_OVERWRITE:
      return network::mojom::CookieChangeCause::EXPIRED_OVERWRITE;
  }
  NOTREACHED();
  return network::mojom::CookieChangeCause::EXPLICIT;
}

}  // namespace

CookieManager::ListenerRegistration::ListenerRegistration() {}

CookieManager::ListenerRegistration::~ListenerRegistration() {}

void CookieManager::ListenerRegistration::DispatchCookieStoreChange(
    const net::CanonicalCookie& cookie,
    net::CookieChangeCause cause) {
  listener->OnCookieChange(cookie, ChangeCauseTranslation(cause));
}

CookieManager::CookieManager(net::CookieStore* cookie_store)
    : cookie_store_(cookie_store) {}

CookieManager::~CookieManager() {}

void CookieManager::AddRequest(network::mojom::CookieManagerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void CookieManager::GetAllCookies(GetAllCookiesCallback callback) {
  cookie_store_->GetAllCookiesAsync(std::move(callback));
}

void CookieManager::GetCookieList(const GURL& url,
                                  const net::CookieOptions& cookie_options,
                                  GetCookieListCallback callback) {
  cookie_store_->GetCookieListWithOptionsAsync(url, cookie_options,
                                               std::move(callback));
}

void CookieManager::SetCanonicalCookie(const net::CanonicalCookie& cookie,
                                       bool secure_source,
                                       bool modify_http_only,
                                       SetCanonicalCookieCallback callback) {
  cookie_store_->SetCanonicalCookieAsync(
      std::make_unique<net::CanonicalCookie>(cookie), secure_source,
      modify_http_only, std::move(callback));
}

void CookieManager::DeleteCookies(
    network::mojom::CookieDeletionFilterPtr filter,
    DeleteCookiesCallback callback) {
  cookie_store_->DeleteAllMatchingInfoAsync(
      DeletionFilterToInfo(std::move(filter)), std::move(callback));
}

void CookieManager::AddCookieChangeListener(
    const GURL& url,
    const std::string& name,
    network::mojom::CookieChangeListenerPtr listener) {
  auto listener_registration = std::make_unique<ListenerRegistration>();
  listener_registration->listener = std::move(listener);

  listener_registration->subscription =
      cookie_store_->GetChangeDispatcher().AddCallbackForCookie(
          url, name,
          base::BindRepeating(
              &CookieManager::ListenerRegistration::DispatchCookieStoreChange,
              // base::Unretained is safe as destruction of the
              // ListenerRegistration will also destroy the
              // CookieChangedSubscription, unregistering the callback.
              base::Unretained(listener_registration.get())));

  listener_registration->listener.set_connection_error_handler(
      base::BindOnce(&CookieManager::RemoveChangeListener,
                     // base::Unretained is safe as destruction of the
                     // CookieManager will also destroy the
                     // notifications_registered list (which this object will be
                     // inserted into, below), which will destroy the
                     // listener, rendering this callback moot.
                     base::Unretained(this),
                     // base::Unretained is safe as destruction of the
                     // ListenerRegistration will also destroy the
                     // CookieChangedSubscription, unregistering the callback.
                     base::Unretained(listener_registration.get())));

  listener_registrations_.push_back(std::move(listener_registration));
}

void CookieManager::AddGlobalChangeListener(
    network::mojom::CookieChangeListenerPtr listener) {
  auto listener_registration = std::make_unique<ListenerRegistration>();
  listener_registration->listener = std::move(listener);

  listener_registration->subscription =
      cookie_store_->GetChangeDispatcher().AddCallbackForAllChanges(
          base::BindRepeating(
              &CookieManager::ListenerRegistration::DispatchCookieStoreChange,
              // base::Unretained is safe as destruction of the
              // ListenerRegistration will also destroy the
              // CookieChangedSubscription, unregistering the callback.
              base::Unretained(listener_registration.get())));

  listener_registration->listener.set_connection_error_handler(
      base::BindOnce(&CookieManager::RemoveChangeListener,
                     // base::Unretained is safe as destruction of the
                     // CookieManager will also destroy the
                     // notifications_registered list (which this object will be
                     // inserted into, below), which will destroy the
                     // listener, rendering this callback moot.
                     base::Unretained(this),
                     // base::Unretained is safe as destruction of the
                     // ListenerRegistration will also destroy the
                     // CookieChangedSubscription, unregistering the callback.
                     base::Unretained(listener_registration.get())));

  listener_registrations_.push_back(std::move(listener_registration));
}

void CookieManager::RemoveChangeListener(ListenerRegistration* registration) {
  for (auto it = listener_registrations_.begin();
       it != listener_registrations_.end(); ++it) {
    if (it->get() == registration) {
      // It isn't expected this will be a common enough operation for
      // the performance of std::vector::erase() to matter.
      listener_registrations_.erase(it);
      return;
    }
  }
  // A broken connection error should never be raised for an unknown pipe.
  NOTREACHED();
}

void CookieManager::CloneInterface(
    network::mojom::CookieManagerRequest new_interface) {
  AddRequest(std::move(new_interface));
}

void CookieManager::FlushCookieStore(FlushCookieStoreCallback callback) {
  // Flushes the backing store (if any) to disk.
  cookie_store_->FlushStore(std::move(callback));
}

CookieDeletionInfo DeletionFilterToInfo(
    network::mojom::CookieDeletionFilterPtr filter) {
  CookieDeletionInfo delete_info;

  if (filter->created_after_time.has_value() &&
      !filter->created_after_time.value().is_null()) {
    delete_info.creation_range.SetStart(filter->created_after_time.value());
  }
  if (filter->created_before_time.has_value() &&
      !filter->created_before_time.value().is_null()) {
    delete_info.creation_range.SetEnd(filter->created_before_time.value());
  }
  delete_info.name = filter->cookie_name;
  delete_info.url = filter->url;

  switch (filter->session_control) {
    case network::mojom::CookieDeletionSessionControl::IGNORE_CONTROL:
      delete_info.session_control = CookieDeleteSessionControl::IGNORE_CONTROL;
      break;
    case network::mojom::CookieDeletionSessionControl::SESSION_COOKIES:
      delete_info.session_control = CookieDeleteSessionControl::SESSION_COOKIES;
      break;
    case network::mojom::CookieDeletionSessionControl::PERSISTENT_COOKIES:
      delete_info.session_control =
          CookieDeleteSessionControl::PERSISTENT_COOKIES;
      break;
  }

  if (filter->including_domains.has_value()) {
    delete_info.domains_and_ips_to_delete.insert(
        filter->including_domains.value().begin(),
        filter->including_domains.value().end());
  }
  if (filter->excluding_domains.has_value()) {
    delete_info.domains_and_ips_to_ignore.insert(
        filter->excluding_domains.value().begin(),
        filter->excluding_domains.value().end());
  }
  if (filter->url.has_value()) {
    // Options to use for deletion of cookies associated with
    // a particular URL.  These options will make sure that all
    // cookies associated with the URL are deleted.
    delete_info.cookie_options.set_include_httponly();
    delete_info.cookie_options.set_same_site_cookie_mode(
        net::CookieOptions::SameSiteCookieMode::INCLUDE_STRICT_AND_LAX);
  }

  return delete_info;
}

}  // namespace network
