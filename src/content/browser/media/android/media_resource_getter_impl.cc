// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/android/media_resource_getter_impl.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/fileapi/browser_file_system_helper.h"
#include "content/browser/resource_context_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "media/base/android/media_url_interceptor.h"
#include "net/base/auth.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_store.h"
#include "net/http/http_auth.h"
#include "net/http/http_transaction_factory.h"
#include "url/gurl.h"

namespace content {

namespace {

// Returns the cookie service for the |browser_context| at the client end of the
// mojo pipe.
network::mojom::CookieManager* GetCookieServiceForContext(
    BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return BrowserContext::GetDefaultStoragePartition(browser_context)
      ->GetCookieManagerForBrowserProcess();
}

void ReturnResultOnUIThread(
    base::OnceCallback<void(const std::string&)> callback,
    const std::string& result) {
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(std::move(callback), result));
}

// Checks the policy for get cookies and returns the cookie line if allowed.
std::string GetCookiesOnIO(const GURL& url,
                           const GURL& site_for_cookies,
                           content::ResourceContext* resource_context,
                           int render_process_id,
                           int render_frame_id,
                           const net::CookieList& cookie_list) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!GetContentClient()->browser()->AllowGetCookie(
          url, site_for_cookies, cookie_list, resource_context,
          render_process_id, render_frame_id)) {
    return std::string();
  }

  return net::CanonicalCookie::BuildCookieLine(cookie_list);
}

void CheckPolicyForCookies(const GURL& url,
                           const GURL& site_for_cookies,
                           content::ResourceContext* resource_context,
                           int render_process_id,
                           int render_frame_id,
                           MediaResourceGetterImpl::GetCookieCB callback,
                           const net::CookieList& cookie_list,
                           const net::CookieStatusList& excluded_cookies) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // AllowGetCookie has to be called on IO thread.
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&GetCookiesOnIO, url, site_for_cookies, resource_context,
                     render_process_id, render_frame_id, cookie_list),
      std::move(callback));
}

void OnSyncGetPlatformPathDone(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    media::MediaResourceGetter::GetPlatformPathCB callback,
    const base::FilePath& platform_path) {
  DCHECK(file_system_context->default_file_task_runner()
             ->RunsTasksInCurrentSequence());

  base::FilePath data_storage_path;
  base::PathService::Get(base::DIR_ANDROID_APP_DATA, &data_storage_path);
  if (data_storage_path.IsParent(platform_path))
    ReturnResultOnUIThread(std::move(callback), platform_path.value());
  else
    ReturnResultOnUIThread(std::move(callback), std::string());
}

void RequestPlatformPathFromFileSystemURL(
    const GURL& url,
    int render_process_id,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    media::MediaResourceGetter::GetPlatformPathCB callback) {
  DCHECK(file_system_context->default_file_task_runner()
             ->RunsTasksInCurrentSequence());
  SyncGetPlatformPath(file_system_context.get(), render_process_id, url,
                      base::BindOnce(&OnSyncGetPlatformPathDone,
                                     file_system_context, std::move(callback)));
}

}  // namespace

MediaResourceGetterImpl::MediaResourceGetterImpl(
    BrowserContext* browser_context,
    storage::FileSystemContext* file_system_context,
    int render_process_id,
    int render_frame_id)
    : browser_context_(browser_context),
      file_system_context_(file_system_context),
      render_process_id_(render_process_id),
      render_frame_id_(render_frame_id),
      weak_factory_(this) {
}

MediaResourceGetterImpl::~MediaResourceGetterImpl() {}

void MediaResourceGetterImpl::GetAuthCredentials(
    const GURL& url,
    GetAuthCredentialsCB callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Non-standard URLs, such as data, will not be found in HTTP auth cache
  // anyway, because they have no valid origin, so don't waste the time.
  if (!url.IsStandard()) {
    GetAuthCredentialsCallback(std::move(callback), base::nullopt);
    return;
  }

  BrowserContext::GetDefaultStoragePartition(browser_context_)
      ->GetNetworkContext()
      ->LookupBasicAuthCredentials(
          url,
          base::BindOnce(&MediaResourceGetterImpl::GetAuthCredentialsCallback,
                         weak_factory_.GetWeakPtr(), std::move(callback)));
}

void MediaResourceGetterImpl::GetCookies(const GURL& url,
                                         const GURL& site_for_cookies,
                                         GetCookieCB callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanAccessDataForOrigin(render_process_id_, url)) {
    // Running the callback asynchronously on the caller thread to avoid
    // reentrancy issues.
    ReturnResultOnUIThread(std::move(callback), std::string());
    return;
  }

  net::CookieOptions options;
  options.set_include_httponly();
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT);
  options.set_do_not_update_access_time();
  GetCookieServiceForContext(browser_context_)
      ->GetCookieList(
          url, options,
          base::BindOnce(&CheckPolicyForCookies, url, site_for_cookies,
                         browser_context_->GetResourceContext(),
                         render_process_id_, render_frame_id_,
                         std::move(callback)));
}

void MediaResourceGetterImpl::GetAuthCredentialsCallback(
    GetAuthCredentialsCB callback,
    const base::Optional<net::AuthCredentials>& credentials) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (credentials)
    std::move(callback).Run(credentials->username(), credentials->password());
  else
    std::move(callback).Run(base::string16(), base::string16());
}

void MediaResourceGetterImpl::GetPlatformPathFromURL(
    const GURL& url,
    GetPlatformPathCB callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(url.SchemeIsFileSystem());

  GetPlatformPathCB cb =
      base::BindOnce(&MediaResourceGetterImpl::GetPlatformPathCallback,
                     weak_factory_.GetWeakPtr(), std::move(callback));

  scoped_refptr<storage::FileSystemContext> context(file_system_context_);
  context->default_file_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&RequestPlatformPathFromFileSystemURL, url,
                                render_process_id_, context, std::move(cb)));
}

void MediaResourceGetterImpl::GetPlatformPathCallback(
    GetPlatformPathCB callback,
    const std::string& platform_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(callback).Run(platform_path);
}

}  // namespace content
