// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/browser/content_settings_manager_impl.h"

#include "base/memory/ptr_util.h"
#include "components/content_settings/browser/tab_specific_content_settings.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

using content_settings::TabSpecificContentSettings;

namespace content_settings {
namespace {

void OnStorageAccessed(int process_id,
                       int frame_id,
                       const GURL& origin_url,
                       const GURL& top_origin_url,
                       bool blocked_by_policy,
                       page_load_metrics::StorageType storage_type) {
  content::RenderFrameHost* frame =
      content::RenderFrameHost::FromID(process_id, frame_id);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(frame);
  if (!web_contents)
    return;

  page_load_metrics::MetricsWebContentsObserver* metrics_observer =
      page_load_metrics::MetricsWebContentsObserver::FromWebContents(
          web_contents);
  if (metrics_observer) {
    metrics_observer->OnStorageAccessed(origin_url, top_origin_url,
                                        blocked_by_policy, storage_type);
  }
}

void OnDomStorageAccessed(int process_id,
                          int frame_id,
                          const GURL& origin_url,
                          const GURL& top_origin_url,
                          bool local,
                          bool blocked_by_policy) {
  content::RenderFrameHost* frame =
      content::RenderFrameHost::FromID(process_id, frame_id);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(frame);
  if (!web_contents)
    return;

  TabSpecificContentSettings* tab_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (tab_settings)
    tab_settings->OnDomStorageAccessed(origin_url, local, blocked_by_policy);
}

}  // namespace

ContentSettingsManagerImpl::~ContentSettingsManagerImpl() = default;

// static
void ContentSettingsManagerImpl::Create(
    content::RenderProcessHost* render_process_host,
    mojo::PendingReceiver<content_settings::mojom::ContentSettingsManager>
        receiver,
    std::unique_ptr<Delegate> delegate) {
  mojo::MakeSelfOwnedReceiver(base::WrapUnique(new ContentSettingsManagerImpl(
                                  render_process_host, std::move(delegate))),
                              std::move(receiver));
}

void ContentSettingsManagerImpl::Clone(
    mojo::PendingReceiver<content_settings::mojom::ContentSettingsManager>
        receiver) {
  mojo::MakeSelfOwnedReceiver(
      base::WrapUnique(new ContentSettingsManagerImpl(*this)),
      std::move(receiver));
}

void ContentSettingsManagerImpl::AllowStorageAccess(
    int32_t render_frame_id,
    StorageType storage_type,
    const url::Origin& origin,
    const GURL& site_for_cookies,
    const url::Origin& top_frame_origin,
    base::OnceCallback<void(bool)> callback) {
  GURL url = origin.GetURL();

  bool allowed = cookie_settings_->IsCookieAccessAllowed(url, site_for_cookies,
                                                         top_frame_origin);
  if (delegate_->AllowStorageAccess(render_process_id_, render_frame_id,
                                    storage_type, url, allowed, &callback)) {
    DCHECK(!callback);
    return;
  }

  switch (storage_type) {
    case StorageType::DATABASE:
      TabSpecificContentSettings::WebDatabaseAccessed(
          render_process_id_, render_frame_id, url, !allowed);
      break;
    case StorageType::LOCAL_STORAGE:
      OnDomStorageAccessed(render_process_id_, render_frame_id, url,
                           top_frame_origin.GetURL(), true, !allowed);
      OnStorageAccessed(render_process_id_, render_frame_id, url,
                        top_frame_origin.GetURL(), !allowed,
                        page_load_metrics::StorageType::kLocalStorage);
      break;
    case StorageType::SESSION_STORAGE:
      OnDomStorageAccessed(render_process_id_, render_frame_id, url,
                           top_frame_origin.GetURL(), false, !allowed);
      OnStorageAccessed(render_process_id_, render_frame_id, url,
                        top_frame_origin.GetURL(), !allowed,
                        page_load_metrics::StorageType::kSessionStorage);

      break;
    case StorageType::FILE_SYSTEM:
      TabSpecificContentSettings::FileSystemAccessed(
          render_process_id_, render_frame_id, url, !allowed);
      OnStorageAccessed(render_process_id_, render_frame_id, url,
                        top_frame_origin.GetURL(), !allowed,
                        page_load_metrics::StorageType::kFileSystem);
      break;
    case StorageType::INDEXED_DB:
      TabSpecificContentSettings::IndexedDBAccessed(
          render_process_id_, render_frame_id, url, !allowed);
      OnStorageAccessed(render_process_id_, render_frame_id, url,
                        top_frame_origin.GetURL(), !allowed,
                        page_load_metrics::StorageType::kIndexedDb);
      break;
    case StorageType::CACHE:
      TabSpecificContentSettings::CacheStorageAccessed(
          render_process_id_, render_frame_id, url, !allowed);
      OnStorageAccessed(render_process_id_, render_frame_id, url,
                        top_frame_origin.GetURL(), !allowed,
                        page_load_metrics::StorageType::kCacheStorage);
      break;
    case StorageType::WEB_LOCKS:
      // State not persisted, no need to record anything.
      break;
  }

  std::move(callback).Run(allowed);
}

void ContentSettingsManagerImpl::OnContentBlocked(int32_t render_frame_id,
                                                  ContentSettingsType type) {
  content::RenderFrameHost* frame =
      content::RenderFrameHost::FromID(render_process_id_, render_frame_id);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(frame);
  if (!web_contents)
    return;

  TabSpecificContentSettings* tab_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (tab_settings)
    tab_settings->OnContentBlocked(type);
}

ContentSettingsManagerImpl::ContentSettingsManagerImpl(
    content::RenderProcessHost* render_process_host,
    std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)),
      render_process_id_(render_process_host->GetID()),
      cookie_settings_(delegate_->GetCookieSettings(
          render_process_host->GetBrowserContext())) {}

ContentSettingsManagerImpl::ContentSettingsManagerImpl(
    const ContentSettingsManagerImpl& other)
    : delegate_(other.delegate_->Clone()),
      render_process_id_(other.render_process_id_),
      cookie_settings_(other.cookie_settings_) {}

}  // namespace content_settings
