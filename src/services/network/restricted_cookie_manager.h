// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_RESTRICTED_COOKIE_MANAGER_H_
#define SERVICES_NETWORK_RESTRICTED_COOKIE_MANAGER_H_

#include <string>

#include "base/component_export.h"
#include "base/containers/linked_list.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_change_dispatcher.h"
#include "net/cookies/cookie_store.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {
class CookieStore;
}  // namespace net

namespace network {

namespace mojom {
class NetworkContextClient;
}  // namespace mojom

class CookieSettings;

// RestrictedCookieManager implementation.
//
// Instances of this class must be created and used on the sequence that hosts
// the CookieStore passed to the constructor.
class COMPONENT_EXPORT(NETWORK_SERVICE) RestrictedCookieManager
    : public mojom::RestrictedCookieManager {
 public:
  // All the pointers passed to the constructor are expected to point to
  // objects that will outlive |this|.
  //
  // |is_service_worker|, |process_id| and |frame_id| will be used when
  // reporting activity to |network_context_client|.
  RestrictedCookieManager(mojom::RestrictedCookieManagerRole role,
                          net::CookieStore* cookie_store,
                          const CookieSettings* cookie_settings,
                          const url::Origin& origin,
                          mojom::NetworkContextClient* network_context_client,
                          bool is_service_worker,
                          int32_t process_id,
                          int32_t frame_id);

  ~RestrictedCookieManager() override;

  const CookieSettings* cookie_settings() const { return cookie_settings_; }

  void GetAllForUrl(const GURL& url,
                    const GURL& site_for_cookies,
                    mojom::CookieManagerGetOptionsPtr options,
                    GetAllForUrlCallback callback) override;

  void SetCanonicalCookie(const net::CanonicalCookie& cookie,
                          const GURL& url,
                          const GURL& site_for_cookies,
                          SetCanonicalCookieCallback callback) override;

  void AddChangeListener(const GURL& url,
                         const GURL& site_for_cookies,
                         mojom::CookieChangeListenerPtr listener,
                         AddChangeListenerCallback callback) override;

  void SetCookieFromString(const GURL& url,
                           const GURL& site_for_cookies,
                           const std::string& cookie,
                           SetCookieFromStringCallback callback) override;

  void GetCookiesString(const GURL& url,
                        const GURL& site_for_cookies,
                        GetCookiesStringCallback callback) override;
  void CookiesEnabledFor(const GURL& url,
                         const GURL& site_for_cookies,
                         CookiesEnabledForCallback callback) override;

 private:
  // The state associated with a CookieChangeListener.
  class Listener;

  // Feeds a net::CookieList to a GetAllForUrl() callback.
  void CookieListToGetAllForUrlCallback(
      const GURL& url,
      const GURL& site_for_cookies,
      const net::CookieOptions& net_options,
      mojom::CookieManagerGetOptionsPtr options,
      GetAllForUrlCallback callback,
      const net::CookieList& cookie_list,
      const net::CookieStatusList& excluded_cookies);

  // Reports the result of setting the cookie to |network_context_client_|, and
  // invokes the user callback.
  void SetCanonicalCookieResult(
      const GURL& url,
      const GURL& site_for_cookies,
      const net::CanonicalCookie& cookie,
      const net::CookieOptions& net_options,
      SetCanonicalCookieCallback user_callback,
      net::CanonicalCookie::CookieInclusionStatus status);

  // Called when the Mojo pipe associated with a listener is closed.
  void RemoveChangeListener(Listener* listener);

  // Ensures that this instance may access the cookies for a given URL.
  //
  // Returns true if the access should be allowed, or false if it should be
  // blocked.
  //
  // If the access would not be allowed, this helper calls
  // mojo::ReportBadMessage(), which closes the pipe.
  bool ValidateAccessToCookiesAt(const GURL& url);

  const mojom::RestrictedCookieManagerRole role_;
  net::CookieStore* const cookie_store_;
  const CookieSettings* const cookie_settings_;
  const url::Origin origin_;
  mojom::NetworkContextClient* const network_context_client_;
  const bool is_service_worker_;
  const int32_t process_id_;
  const int32_t frame_id_;

  base::LinkedList<Listener> listeners_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<RestrictedCookieManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RestrictedCookieManager);
};

}  // namespace network

#endif  // SERVICES_NETWORK_RESTRICTED_COOKIE_MANAGER_H_
