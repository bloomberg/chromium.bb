// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cookie_manager.h"

#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_options.h"
#include "url/gurl.h"

namespace network {

namespace {

// Class to wrap a CookieDeletionFilterPtr and provide a predicate for
// use by DeleteAllCreatedBetweenWithPredicateAsync.
class PredicateWrapper {
 public:
  explicit PredicateWrapper(network::mojom::CookieDeletionFilterPtr filter)
      : use_excluding_domains_(filter->excluding_domains.has_value()),
        excluding_domains_(filter->excluding_domains.has_value()
                               ? std::set<std::string>(
                                     filter->excluding_domains.value().begin(),
                                     filter->excluding_domains.value().end())
                               : std::set<std::string>()),
        use_including_domains_(filter->including_domains.has_value()),
        including_domains_(filter->including_domains.has_value()
                               ? std::set<std::string>(
                                     filter->including_domains.value().begin(),
                                     filter->including_domains.value().end())
                               : std::set<std::string>()),
        use_cookie_name_(filter->cookie_name.has_value()),
        cookie_name_(filter->cookie_name.has_value()
                         ? filter->cookie_name.value()
                         : std::string()),
        use_url_(filter->url.has_value()),
        url_(filter->url.has_value() ? filter->url.value() : GURL()),
        session_control_(filter->session_control) {
    // Options to use for deletion of cookies associated with
    // a particular URL.  These options will make sure that all
    // cookies associated with the URL are deleted.
    const_cast<net::CookieOptions&>(options_).set_include_httponly();
    const_cast<net::CookieOptions&>(options_).set_same_site_cookie_mode(
        net::CookieOptions::SameSiteCookieMode::INCLUDE_STRICT_AND_LAX);
  }

  // Return true if the given cookie should be deleted.
  bool Predicate(const net::CanonicalCookie& cookie) {
    // Ignore begin/end times; they're handled by method args.
    // Deleted cookies must satisfy all conditions, so if any of the
    // below matches fail return false early.

    // Delete if the cookie is not in excluding_domains_.
    if (use_excluding_domains_ && DomainMatches(cookie, excluding_domains_))
      return false;

    // Delete if the cookie is in including_domains_.
    if (use_including_domains_ && !DomainMatches(cookie, including_domains_))
      return false;

    // Delete if the cookie has a specified name.
    if (use_cookie_name_ && !(cookie_name_ == cookie.Name()))
      return false;

    // Delete if the cookie matches the URL.
    if (use_url_ && !cookie.IncludeForRequestURL(url_, options_))
      return false;

    // Delete if the cookie is not the correct persistent or session type.
    if (session_control_ !=
            network::mojom::CookieDeletionSessionControl::IGNORE_CONTROL &&
        (cookie.IsPersistent() !=
         (session_control_ ==
          network::mojom::CookieDeletionSessionControl::PERSISTENT_COOKIES))) {
      return false;
    }

    return true;
  }

 private:
  // Return true if the eTLD+1 of the domain matches any of the strings
  // in |match_domains|, false otherwise.
  bool DomainMatches(const net::CanonicalCookie& cookie,
                     const std::set<std::string>& match_domains) {
    std::string effective_domain(
        net::registry_controlled_domains::GetDomainAndRegistry(
            // GetDomainAndRegistry() is insensitive to leading dots, i.e.
            // to host/domain cookie distinctions.
            cookie.Domain(),
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES));
    // If the cookie's domain is is not parsed as belonging to a registry
    // (e.g. for IP addresses or internal hostnames) an empty string will be
    // returned.  In this case, use the domain in the cookie.
    if (effective_domain.empty())
      effective_domain = cookie.Domain();

    return match_domains.count(effective_domain) != 0;
  }

  const bool use_excluding_domains_;
  const std::set<std::string> excluding_domains_;

  const bool use_including_domains_;
  const std::set<std::string> including_domains_;

  const bool use_cookie_name_;
  const std::string cookie_name_;

  const bool use_url_;
  const GURL url_;

  const network::mojom::CookieDeletionSessionControl session_control_;

  // Set at construction; used for IncludeForRequestURL().
  const net::CookieOptions options_;

  DISALLOW_COPY_AND_ASSIGN(PredicateWrapper);
};

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
  base::Time start_time;
  base::Time end_time;

  if (filter->created_after_time.has_value())
    start_time = filter->created_after_time.value();

  if (filter->created_before_time.has_value())
    end_time = filter->created_before_time.value();

  cookie_store_->DeleteAllCreatedBetweenWithPredicateAsync(
      start_time, end_time,
      base::BindRepeating(
          &PredicateWrapper::Predicate,
          std::make_unique<PredicateWrapper>(std::move(filter))),
      std::move(callback));
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

}  // namespace network
