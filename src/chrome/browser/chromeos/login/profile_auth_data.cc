// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/profile_auth_data.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/cookies/canonical_cookie.h"
#include "net/http/http_auth_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_transaction_factory.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/channel_id_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace chromeos {

namespace {

const char kSAMLStartCookie[] = "google-accounts-saml-start";
const char kSAMLEndCookie[] = "google-accounts-saml-end";

// Given a |cookie| set during login, returns true if the cookie may have been
// set by GAIA. The main criterion is the |cookie|'s creation date. The points
// in time at which redirects from GAIA to SAML IdP and back occur are stored
// in |saml_start_time| and |saml_end_time|. If the cookie was set between
// these two times, it was created by the SAML IdP. Otherwise, it was created
// by GAIA.
// As an additional precaution, the cookie's domain is checked. If the domain
// contains "google" or "youtube", the cookie is considered to have been set
// by GAIA as well.
bool IsGAIACookie(const base::Time& saml_start_time,
                  const base::Time& saml_end_time,
                  const net::CanonicalCookie& cookie) {
  const base::Time& creation_date = cookie.CreationDate();
  if (creation_date < saml_start_time)
    return true;
  if (!saml_end_time.is_null() && creation_date > saml_end_time)
    return true;

  const std::string& domain = cookie.Domain();
  return domain.find("google") != std::string::npos ||
         domain.find("youtube") != std::string::npos;
}

class ProfileAuthDataTransferer
    : public base::RefCountedDeleteOnSequence<ProfileAuthDataTransferer> {
 public:
  ProfileAuthDataTransferer(
      content::StoragePartition* from_partition,
      content::StoragePartition* to_partition,
      bool transfer_auth_cookies_and_channel_ids_on_first_login,
      bool transfer_saml_auth_cookies_on_subsequent_login,
      const base::Closure& completion_callback);

  void BeginTransfer();

 private:
  friend class RefCountedDeleteOnSequence<ProfileAuthDataTransferer>;
  friend class base::DeleteHelper<ProfileAuthDataTransferer>;

  ~ProfileAuthDataTransferer();

  // Transfer the proxy auth cache from |from_context_| to |to_context_|. If
  // the user was required to authenticate with a proxy during login, this
  // authentication information will be transferred into the user's session.
  void TransferProxyAuthCache();

  // Callback that receives the content of |to_partition_|'s cookie jar. Checks
  // whether this is the user's first login, based on the state of the cookie
  // jar, and starts retrieval of the data that should be transfered.
  void OnTargetCookieJarContentsRetrieved(
      const net::CookieList& target_cookies);

  // Retrieve the contents of |from_partition_|'s cookie jar. When the retrieval
  // finishes, OnCookiesToTransferRetrieved will be called with the result.
  void RetrieveCookiesToTransfer();

  // Callback that receives the contents of |from_partition_|'s cookie jar.
  // Transfers the necessary cookies to |to_partition_|'s cookie jar.
  void OnCookiesToTransferRetrieved(const net::CookieList& cookies_to_transfer);

  // Imports |cookies| into |to_partition_|'s cookie jar. |cookie.IsCanonical()|
  // must be true for all cookies in |cookies|.
  void ImportCookies(const net::CookieList& cookies);
  void OnCookieSet(bool result);

  // Retrieve |from_context_|'s channel IDs. When the retrieval finishes,
  // OnChannelIDsToTransferRetrieved will be called with the result.
  void RetrieveChannelIDsToTransfer();

  // Callback that receives |from_context_|'s channel IDs and transfers them to
  // |to_context_|.
  void OnChannelIDsToTransferRetrieved(
      const net::ChannelIDStore::ChannelIDList& channel_ids_to_transfer);

  content::StoragePartition* from_partition_;
  scoped_refptr<net::URLRequestContextGetter> from_context_;
  content::StoragePartition* to_partition_;
  scoped_refptr<net::URLRequestContextGetter> to_context_;
  bool transfer_auth_cookies_and_channel_ids_on_first_login_;
  bool transfer_saml_auth_cookies_on_subsequent_login_;
  base::OnceClosure completion_callback_;

  bool first_login_ = false;
};

ProfileAuthDataTransferer::ProfileAuthDataTransferer(
    content::StoragePartition* from_partition,
    content::StoragePartition* to_partition,
    bool transfer_auth_cookies_and_channel_ids_on_first_login,
    bool transfer_saml_auth_cookies_on_subsequent_login,
    const base::Closure& completion_callback)
    : RefCountedDeleteOnSequence<ProfileAuthDataTransferer>(
          base::ThreadTaskRunnerHandle::Get()),
      from_partition_(from_partition),
      from_context_(from_partition->GetURLRequestContext()),
      to_partition_(to_partition),
      to_context_(to_partition->GetURLRequestContext()),
      transfer_auth_cookies_and_channel_ids_on_first_login_(
          transfer_auth_cookies_and_channel_ids_on_first_login),
      transfer_saml_auth_cookies_on_subsequent_login_(
          transfer_saml_auth_cookies_on_subsequent_login),
      completion_callback_(completion_callback) {}

ProfileAuthDataTransferer::~ProfileAuthDataTransferer() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!completion_callback_.is_null())
    std::move(completion_callback_).Run();
}

void ProfileAuthDataTransferer::BeginTransfer() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&ProfileAuthDataTransferer::TransferProxyAuthCache, this));

  if (transfer_auth_cookies_and_channel_ids_on_first_login_ ||
      transfer_saml_auth_cookies_on_subsequent_login_) {
    // Retrieve the contents of |to_partition_|'s cookie jar.
    network::mojom::CookieManager* to_manager =
        to_partition_->GetCookieManagerForBrowserProcess();
    to_manager->GetAllCookies(base::BindOnce(
        &ProfileAuthDataTransferer::OnTargetCookieJarContentsRetrieved, this));
  }
}

void ProfileAuthDataTransferer::TransferProxyAuthCache() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::HttpAuthCache* new_cache = to_context_->GetURLRequestContext()
                                      ->http_transaction_factory()
                                      ->GetSession()
                                      ->http_auth_cache();
  new_cache->UpdateAllFrom(*from_context_->GetURLRequestContext()
                                ->http_transaction_factory()
                                ->GetSession()
                                ->http_auth_cache());
}

void ProfileAuthDataTransferer::OnTargetCookieJarContentsRetrieved(
    const net::CookieList& target_cookies) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  bool transfer_auth_cookies = false;
  bool transfer_channel_ids = false;

  first_login_ = target_cookies.empty();
  if (first_login_) {
    // On first login, transfer all auth cookies and channel IDs if
    // |transfer_auth_cookies_and_channel_ids_on_first_login_| is true.
    transfer_auth_cookies =
        transfer_auth_cookies_and_channel_ids_on_first_login_;
    transfer_channel_ids =
        transfer_auth_cookies_and_channel_ids_on_first_login_;
  } else {
    // On subsequent login, transfer auth cookies set by the SAML IdP if
    // |transfer_saml_auth_cookies_on_subsequent_login_| is true.
    transfer_auth_cookies = transfer_saml_auth_cookies_on_subsequent_login_;
  }

  if (transfer_auth_cookies)
    RetrieveCookiesToTransfer();

  if (transfer_channel_ids) {
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(&ProfileAuthDataTransferer::RetrieveChannelIDsToTransfer,
                       this));
  }
}

void ProfileAuthDataTransferer::RetrieveCookiesToTransfer() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  network::mojom::CookieManager* from_manager =
      from_partition_->GetCookieManagerForBrowserProcess();
  from_manager->GetAllCookies(base::BindOnce(
      &ProfileAuthDataTransferer::OnCookiesToTransferRetrieved, this));
}

void ProfileAuthDataTransferer::OnCookiesToTransferRetrieved(
    const net::CookieList& cookies) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Look for cookies indicating the points in time at which redirects from GAIA
  // to SAML IdP and back occurred. These cookies are synthesized by
  // chrome/browser/resources/gaia_auth/background.js. If the cookies are found,
  // their creation times are noted and the cookies are deleted.
  base::Time saml_start_time;
  base::Time saml_end_time;
  net::CookieList cookies_to_transfer;
  for (const auto& cookie : cookies) {
    if (cookie.Name() == kSAMLStartCookie) {
      saml_start_time = cookie.CreationDate();
    } else if (cookie.Name() == kSAMLEndCookie) {
      saml_end_time = cookie.CreationDate();
    } else {
      cookies_to_transfer.push_back(cookie);
    }
  }

  if (first_login_) {
    ImportCookies(cookies_to_transfer);
  } else {
    net::CookieList non_gaia_cookies;
    for (const auto& cookie : cookies_to_transfer) {
      if (!IsGAIACookie(saml_start_time, saml_end_time, cookie))
        non_gaia_cookies.push_back(cookie);
    }
    ImportCookies(non_gaia_cookies);
  }
}

void ProfileAuthDataTransferer::ImportCookies(const net::CookieList& cookies) {
  network::mojom::CookieManager* cookie_manager =
      to_partition_->GetCookieManagerForBrowserProcess();

  for (const auto& cookie : cookies) {
    // Assume secure_source - since the cookies are being restored from
    // another store, they have already gone through the strict secure check.
    DCHECK(cookie.IsCanonical());
    cookie_manager->SetCanonicalCookie(
        cookie, true /*secure_source*/, true /*modify_http_only*/,
        base::BindOnce(&ProfileAuthDataTransferer::OnCookieSet, this));
  }
}

void ProfileAuthDataTransferer::OnCookieSet(bool result) {
  // This function does nothing but extend the lifetime of |this| until after
  // all cookies have been transferred.
}

void ProfileAuthDataTransferer::RetrieveChannelIDsToTransfer() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::ChannelIDService* from_service =
      from_context_->GetURLRequestContext()->channel_id_service();
  from_service->GetChannelIDStore()->GetAllChannelIDs(base::BindOnce(
      &ProfileAuthDataTransferer::OnChannelIDsToTransferRetrieved, this));
}

void ProfileAuthDataTransferer::OnChannelIDsToTransferRetrieved(
    const net::ChannelIDStore::ChannelIDList& channel_ids_to_transfer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(first_login_);

  net::ChannelIDService* to_cert_service =
      to_context_->GetURLRequestContext()->channel_id_service();
  to_cert_service->GetChannelIDStore()->InitializeFrom(channel_ids_to_transfer);
}

}  // namespace

void ProfileAuthData::Transfer(
    content::StoragePartition* from_partition,
    content::StoragePartition* to_partition,
    bool transfer_auth_cookies_and_channel_ids_on_first_login,
    bool transfer_saml_auth_cookies_on_subsequent_login,
    const base::Closure& completion_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::MakeRefCounted<ProfileAuthDataTransferer>(
      from_partition, to_partition,
      transfer_auth_cookies_and_channel_ids_on_first_login,
      transfer_saml_auth_cookies_on_subsequent_login, completion_callback)
      ->BeginTransfer();
}

}  // namespace chromeos
