// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/restricted_cookie_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "mojo/public/cpp/bindings/message.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_util.h"
#include "services/network/cookie_managers_shared.h"
#include "services/network/cookie_settings.h"

namespace network {

class RestrictedCookieManager::Listener : public base::LinkNode<Listener> {
 public:
  Listener(net::CookieStore* cookie_store,
           const RestrictedCookieManager* restricted_cookie_manager,
           const GURL& url,
           const GURL& site_for_cookies,
           net::CookieOptions options,
           mojom::CookieChangeListenerPtr mojo_listener)
      : restricted_cookie_manager_(restricted_cookie_manager),
        url_(url),
        site_for_cookies_(site_for_cookies),
        options_(options),
        mojo_listener_(std::move(mojo_listener)) {
    // TODO(pwnall): add a constructor w/options to net::CookieChangeDispatcher.
    cookie_store_subscription_ =
        cookie_store->GetChangeDispatcher().AddCallbackForUrl(
            url,
            base::BindRepeating(
                &Listener::OnCookieChange,
                // Safe because net::CookieChangeDispatcher guarantees that the
                // callback will stop being called immediately after we remove
                // the subscription, and the cookie store lives on the same
                // thread as we do.
                base::Unretained(this)));
  }

  ~Listener() { DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_); }

  mojom::CookieChangeListenerPtr& mojo_listener() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return mojo_listener_;
  }

 private:
  // net::CookieChangeDispatcher callback.
  void OnCookieChange(const net::CanonicalCookie& cookie,
                      net::CookieChangeCause cause) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (cookie.IncludeForRequestURL(url_, options_) !=
        net::CanonicalCookie::CookieInclusionStatus::INCLUDE)
      return;

    // When a user blocks a site's access to cookies, the existing cookies are
    // not deleted. This check prevents the site from observing their cookies
    // being deleted at a later time, which can happen due to eviction or due to
    // the user explicitly deleting all cookies.
    if (!restricted_cookie_manager_->cookie_settings()->IsCookieAccessAllowed(
            url_, site_for_cookies_))
      return;

    mojo_listener_->OnCookieChange(cookie, ToCookieChangeCause(cause));
  }

  // The CookieChangeDispatcher subscription used by this listener.
  std::unique_ptr<net::CookieChangeSubscription> cookie_store_subscription_;

  // Raw pointer usage is safe because RestrictedCookieManager owns this
  // instance and is guaranteed to outlive it.
  const RestrictedCookieManager* const restricted_cookie_manager_;

  // The URL whose cookies this listener is interested in.
  const GURL url_;

  // Site context in which we're used; used for permission-checking.
  const GURL site_for_cookies_;

  // CanonicalCookie::IncludeForRequestURL options for this listener's interest.
  const net::CookieOptions options_;

  mojom::CookieChangeListenerPtr mojo_listener_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(Listener);
};

RestrictedCookieManager::RestrictedCookieManager(
    net::CookieStore* cookie_store,
    const CookieSettings* cookie_settings,
    const url::Origin& origin)
    : cookie_store_(cookie_store),
      cookie_settings_(cookie_settings),
      origin_(origin),
      weak_ptr_factory_(this) {
  DCHECK(cookie_store);
}

RestrictedCookieManager::~RestrictedCookieManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::LinkNode<Listener>* node = listeners_.head();
  while (node != listeners_.end()) {
    Listener* listener_reference = node->value();
    node = node->next();
    // The entire list is going away, no need to remove nodes from it.
    delete listener_reference;
  }
}

void RestrictedCookieManager::GetAllForUrl(
    const GURL& url,
    const GURL& site_for_cookies,
    mojom::CookieManagerGetOptionsPtr options,
    GetAllForUrlCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!ValidateAccessToCookiesAt(url)) {
    std::move(callback).Run({});
    return;
  }

  // TODO(morlovich): Try to validate site_for_cookies as well.

  if (!cookie_settings_->IsCookieAccessAllowed(url, site_for_cookies)) {
    std::move(callback).Run({});
    return;
  }

  // TODO(https://crbug.com/925311): Wire initiator here.
  net::CookieOptions net_options;
  net_options.set_same_site_cookie_context(
      net::cookie_util::ComputeSameSiteContextForScriptGet(
          url, site_for_cookies, base::nullopt /*initiator*/));

  cookie_store_->GetCookieListWithOptionsAsync(
      url, net_options,
      base::BindOnce(&RestrictedCookieManager::CookieListToGetAllForUrlCallback,
                     weak_ptr_factory_.GetWeakPtr(), url, site_for_cookies,
                     std::move(options), std::move(callback)));
}

void RestrictedCookieManager::CookieListToGetAllForUrlCallback(
    const GURL& url,
    const GURL& site_for_cookies,
    mojom::CookieManagerGetOptionsPtr options,
    GetAllForUrlCallback callback,
    const net::CookieList& cookie_list,
    const net::CookieStatusList& excluded_cookies) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<net::CanonicalCookie> result;
  result.reserve(cookie_list.size());
  mojom::CookieMatchType match_type = options->match_type;
  const std::string& match_name = options->name;
  for (size_t i = 0; i < cookie_list.size(); ++i) {
    const net::CanonicalCookie& cookie = cookie_list[i];
    const std::string& cookie_name = cookie.Name();

    if (match_type == mojom::CookieMatchType::EQUALS) {
      if (cookie_name != match_name)
        continue;
    } else if (match_type == mojom::CookieMatchType::STARTS_WITH) {
      if (!base::StartsWith(cookie_name, match_name,
                            base::CompareCase::SENSITIVE)) {
        continue;
      }
    } else {
      NOTREACHED();
    }
    result.emplace_back(cookie);
  }
  std::move(callback).Run(std::move(result));
}

void RestrictedCookieManager::SetCanonicalCookie(
    const net::CanonicalCookie& cookie,
    const GURL& url,
    const GURL& site_for_cookies,
    SetCanonicalCookieCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!ValidateAccessToCookiesAt(url)) {
    std::move(callback).Run(false);
    return;
  }

  // TODO(morlovich): Try to validate site_for_cookies as well.
  if (!cookie_settings_->IsCookieAccessAllowed(url, site_for_cookies)) {
    std::move(callback).Run(false);
    return;
  }

  // TODO(pwnall): Validate the CanonicalCookie fields.

  base::Time now = base::Time::NowFromSystemTime();
  auto sanitized_cookie = std::make_unique<net::CanonicalCookie>(
      cookie.Name(), cookie.Value(), cookie.Domain(), cookie.Path(), now,
      cookie.ExpiryDate(), now, cookie.IsSecure(), cookie.IsHttpOnly(),
      cookie.SameSite(), cookie.Priority());

  // TODO(pwnall): source_scheme might depend on the renderer.
  net::CookieOptions options;
  options.set_same_site_cookie_context(
      net::cookie_util::ComputeSameSiteContextForScriptSet(url,
                                                           site_for_cookies));
  options.set_exclude_httponly();  // Default, but make it explicit here.
  cookie_store_->SetCanonicalCookieAsync(
      std::move(sanitized_cookie), origin_.scheme(), options,
      net::cookie_util::AdaptCookieInclusionStatusToBool(std::move(callback)));
}

void RestrictedCookieManager::AddChangeListener(
    const GURL& url,
    const GURL& site_for_cookies,
    mojom::CookieChangeListenerPtr mojo_listener,
    AddChangeListenerCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!ValidateAccessToCookiesAt(url)) {
    std::move(callback).Run();
    return;
  }

  // TODO(https://crbug.com/925311): Wire initiator here.
  net::CookieOptions net_options;
  net_options.set_same_site_cookie_context(
      net::cookie_util::ComputeSameSiteContextForScriptGet(
          url, site_for_cookies, base::nullopt /*initiator*/));

  auto listener =
      std::make_unique<Listener>(cookie_store_, this, url, site_for_cookies,
                                 net_options, std::move(mojo_listener));

  listener->mojo_listener().set_connection_error_handler(
      base::BindOnce(&RestrictedCookieManager::RemoveChangeListener,
                     weak_ptr_factory_.GetWeakPtr(),
                     // Safe because this owns the listener, so the listener is
                     // guaranteed to be alive for as long as the weak pointer
                     // above resolves.
                     base::Unretained(listener.get())));

  // The linked list takes over the Listener ownership.
  listeners_.Append(listener.release());
  std::move(callback).Run();
}

void RestrictedCookieManager::RemoveChangeListener(Listener* listener) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  listener->RemoveFromList();
  delete listener;
}

bool RestrictedCookieManager::ValidateAccessToCookiesAt(const GURL& url) {
  if (origin_.IsSameOriginWith(url::Origin::Create(url)))
    return true;

  mojo::ReportBadMessage("Incorrect url origin");
  return false;
}

}  // namespace network
