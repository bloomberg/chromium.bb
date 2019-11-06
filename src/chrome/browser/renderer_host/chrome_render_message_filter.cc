// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/chrome_render_message_filter.h"

#include <stdint.h>

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/page_load_metrics/metrics_web_contents_observer.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/predictors/preconnect_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/render_messages.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/network_hints/common/network_hints_common.h"
#include "components/network_hints/common/network_hints_messages.h"
#include "components/web_cache/browser/web_cache_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/network_isolation_key.h"
#include "ppapi/buildflags/buildflags.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/guest_view/web_view/web_view_permission_helper.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#include "extensions/common/manifest_handlers/default_locale_handler.h"
#endif

using content::BrowserThread;

namespace {

void OnDomStorageAccessedUI(int process_id,
                            int routing_id,
                            const GURL& origin_url,
                            const GURL& top_origin_url,
                            bool local,
                            bool blocked_by_policy) {
  content::RenderFrameHost* frame =
      content::RenderFrameHost::FromID(process_id, routing_id);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(frame);

  if (!web_contents)
    return;

  TabSpecificContentSettings* tab_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (tab_settings)
    tab_settings->OnDomStorageAccessed(origin_url, local, blocked_by_policy);

  page_load_metrics::MetricsWebContentsObserver* metrics_observer =
      page_load_metrics::MetricsWebContentsObserver::FromWebContents(
          web_contents);
  if (metrics_observer)
    metrics_observer->OnDomStorageAccessed(origin_url, top_origin_url, local,
                                           blocked_by_policy);
}

const uint32_t kRenderFilteredMessageClasses[] = {
    ChromeMsgStart, NetworkHintsMsgStart,
};

void StartPreconnect(
    base::WeakPtr<predictors::PreconnectManager> preconnect_manager,
    int render_process_id,
    int render_frame_id,
    const GURL& url,
    bool allow_credentials) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!preconnect_manager)
    return;

  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (!render_frame_host)
    return;

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents)
    return;

  net::NetworkIsolationKey network_isolation_key(
      web_contents->GetMainFrame()->GetLastCommittedOrigin(),
      render_frame_host->GetLastCommittedOrigin());
  preconnect_manager->StartPreconnectUrl(url, allow_credentials,
                                         network_isolation_key);
}

}  // namespace

ChromeRenderMessageFilter::ChromeRenderMessageFilter(int render_process_id,
                                                     Profile* profile)
    : BrowserMessageFilter(kRenderFilteredMessageClasses,
                           base::size(kRenderFilteredMessageClasses)),
      render_process_id_(render_process_id),
      preconnect_manager_initialized_(false),
      cookie_settings_(CookieSettingsFactory::GetForProfile(profile)) {
  auto* loading_predictor =
      predictors::LoadingPredictorFactory::GetForProfile(profile);
  if (loading_predictor && loading_predictor->preconnect_manager()) {
    preconnect_manager_ = loading_predictor->preconnect_manager()->GetWeakPtr();
    preconnect_manager_initialized_ = true;
  }
}

ChromeRenderMessageFilter::~ChromeRenderMessageFilter() {
}

bool ChromeRenderMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChromeRenderMessageFilter, message)
    IPC_MESSAGE_HANDLER(NetworkHintsMsg_DNSPrefetch, OnDnsPrefetch)
    IPC_MESSAGE_HANDLER(NetworkHintsMsg_Preconnect, OnPreconnect)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_AllowDatabase, OnAllowDatabase)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_AllowDOMStorage, OnAllowDOMStorage)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(
        ChromeViewHostMsg_RequestFileSystemAccessSync,
        OnRequestFileSystemAccessSync)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_RequestFileSystemAccessAsync,
                        OnRequestFileSystemAccessAsync)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_AllowIndexedDB, OnAllowIndexedDB)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_AllowCacheStorage,
                        OnAllowCacheStorage)
#if BUILDFLAG(ENABLE_PLUGINS)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_IsCrashReportingEnabled,
                        OnIsCrashReportingEnabled)
#endif
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void ChromeRenderMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message, BrowserThread::ID* thread) {
#if BUILDFLAG(ENABLE_PLUGINS)
  switch (message.type()) {
    case ChromeViewHostMsg_IsCrashReportingEnabled::ID:
      *thread = BrowserThread::UI;
      break;
    default:
      break;
  }
#endif
}

void ChromeRenderMessageFilter::OnDnsPrefetch(
    const network_hints::LookupRequest& request) {
  if (preconnect_manager_initialized_) {
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&predictors::PreconnectManager::StartPreresolveHosts,
                       preconnect_manager_, request.hostname_list));
  }
}

void ChromeRenderMessageFilter::OnPreconnect(int render_frame_id,
                                             const GURL& url,
                                             bool allow_credentials,
                                             int count) {
  if (count < 1) {
    LOG(WARNING) << "NetworkHintsMsg_Preconnect IPC with invalid count: "
                 << count;
    return;
  }

  if (!url.is_valid() || !url.has_host() || !url.has_scheme() ||
      !url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  if (!preconnect_manager_initialized_)
    return;

  // TODO(mmenke):  Think about enabling cross-site preconnects, though that
  // will result in at least some cross-site information leakage.
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&StartPreconnect, preconnect_manager_, render_process_id_,
                     render_frame_id, url, allow_credentials));
}

void ChromeRenderMessageFilter::OnAllowDatabase(
    int render_frame_id,
    const url::Origin& origin,
    const GURL& site_for_cookies,
    const url::Origin& top_frame_origin,
    bool* allowed) {
  *allowed = cookie_settings_->IsCookieAccessAllowed(
      origin.GetURL(), site_for_cookies, top_frame_origin);
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&TabSpecificContentSettings::WebDatabaseAccessed,
                     render_process_id_, render_frame_id, origin.GetURL(),
                     !*allowed));
}

void ChromeRenderMessageFilter::OnAllowDOMStorage(
    int render_frame_id,
    const url::Origin& origin,
    const GURL& site_for_cookies,
    const url::Origin& top_frame_origin,
    bool local,
    bool* allowed) {
  GURL url = origin.GetURL();
  *allowed = cookie_settings_->IsCookieAccessAllowed(url, site_for_cookies,
                                                     top_frame_origin);
  // Record access to DOM storage for potential display in UI.
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&OnDomStorageAccessedUI, render_process_id_,
                                render_frame_id, url, top_frame_origin.GetURL(),
                                local, !*allowed));
}

void ChromeRenderMessageFilter::OnRequestFileSystemAccessSync(
    int render_frame_id,
    const url::Origin& origin,
    const GURL& site_for_cookies,
    const url::Origin& top_frame_origin,
    IPC::Message* reply_msg) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::Callback<void(bool)> callback = base::Bind(
      &ChromeRenderMessageFilter::OnRequestFileSystemAccessSyncResponse,
      base::WrapRefCounted(this), reply_msg);
  OnRequestFileSystemAccess(render_frame_id, origin, site_for_cookies,
                            top_frame_origin, callback);
}

void ChromeRenderMessageFilter::OnRequestFileSystemAccessSyncResponse(
    IPC::Message* reply_msg,
    bool allowed) {
  ChromeViewHostMsg_RequestFileSystemAccessSync::WriteReplyParams(reply_msg,
                                                                  allowed);
  Send(reply_msg);
}

void ChromeRenderMessageFilter::OnRequestFileSystemAccessAsync(
    int render_frame_id,
    int request_id,
    const url::Origin& origin,
    const GURL& site_for_cookies,
    const url::Origin& top_frame_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::Callback<void(bool)> callback = base::Bind(
      &ChromeRenderMessageFilter::OnRequestFileSystemAccessAsyncResponse,
      base::WrapRefCounted(this), render_frame_id, request_id);
  OnRequestFileSystemAccess(render_frame_id, origin, site_for_cookies,
                            top_frame_origin, callback);
}

void ChromeRenderMessageFilter::OnRequestFileSystemAccessAsyncResponse(
    int render_frame_id,
    int request_id,
    bool allowed) {
  Send(new ChromeViewMsg_RequestFileSystemAccessAsyncResponse(
      render_frame_id, request_id, allowed));
}

void ChromeRenderMessageFilter::OnRequestFileSystemAccess(
    int render_frame_id,
    const url::Origin& origin,
    const GURL& site_for_cookies,
    const url::Origin& top_frame_origin,
    base::Callback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  bool allowed = cookie_settings_->IsCookieAccessAllowed(
      origin.GetURL(), site_for_cookies, top_frame_origin);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  bool is_web_view_guest = extensions::WebViewRendererState::GetInstance()
      ->IsGuest(render_process_id_);
  if (is_web_view_guest) {
    // Record access to file system for potential display in UI.
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&ChromeRenderMessageFilter::FileSystemAccessedOnUIThread,
                       render_process_id_, render_frame_id, origin.GetURL(),
                       allowed, callback));
    return;
  }
#endif
  callback.Run(allowed);
  // Record access to file system for potential display in UI.
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&TabSpecificContentSettings::FileSystemAccessed,
                                render_process_id_, render_frame_id,
                                origin.GetURL(), !allowed));
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void ChromeRenderMessageFilter::FileSystemAccessedOnUIThread(
    int render_process_id,
    int render_frame_id,
    const GURL& url,
    bool allowed,
    base::Callback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  extensions::WebViewPermissionHelper* web_view_permission_helper =
      extensions::WebViewPermissionHelper::FromFrameID(
          render_process_id, render_frame_id);
  // Between the time the permission request is made and the time it is handled
  // by the UI thread, the extensions::WebViewPermissionHelper might be gone.
  if (!web_view_permission_helper)
    return;
  web_view_permission_helper->RequestFileSystemPermission(
      url, allowed,
      base::BindOnce(&ChromeRenderMessageFilter::FileSystemAccessedResponse,
                     render_process_id, render_frame_id, url, callback));
}

void ChromeRenderMessageFilter::FileSystemAccessedResponse(
    int render_process_id,
    int render_frame_id,
    const GURL& url,
    base::Callback<void(bool)> callback,
    bool allowed) {
  TabSpecificContentSettings::FileSystemAccessed(
      render_process_id, render_frame_id, url, !allowed);
  callback.Run(allowed);
}
#endif

void ChromeRenderMessageFilter::OnAllowIndexedDB(
    int render_frame_id,
    const url::Origin& origin,
    const GURL& site_for_cookies,
    const url::Origin& top_frame_origin,
    bool* allowed) {
  *allowed = cookie_settings_->IsCookieAccessAllowed(
      origin.GetURL(), site_for_cookies, top_frame_origin);
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&TabSpecificContentSettings::IndexedDBAccessed,
                                render_process_id_, render_frame_id,
                                origin.GetURL(), !*allowed));
}

void ChromeRenderMessageFilter::OnAllowCacheStorage(
    int render_frame_id,
    const url::Origin& origin,
    const GURL& site_for_cookies,
    const url::Origin& top_frame_origin,
    bool* allowed) {
  GURL url = origin.GetURL();
  *allowed = cookie_settings_->IsCookieAccessAllowed(url, site_for_cookies,
                                                     top_frame_origin);
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&TabSpecificContentSettings::CacheStorageAccessed,
                     render_process_id_, render_frame_id, url, !*allowed));
}

#if BUILDFLAG(ENABLE_PLUGINS)
void ChromeRenderMessageFilter::OnIsCrashReportingEnabled(bool* enabled) {
  *enabled = ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();
}
#endif
