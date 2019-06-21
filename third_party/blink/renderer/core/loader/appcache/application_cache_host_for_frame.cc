// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/appcache/application_cache_host_for_frame.h"

#include "third_party/blink/public/common/loader/url_loader_factory_bundle.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/web_test_support.h"

namespace blink {

namespace {

const char kHttpGETMethod[] = "GET";

KURL ClearUrlRef(const KURL& input_url) {
  KURL url(input_url);
  if (!url.HasFragmentIdentifier())
    return url;
  url.RemoveFragmentIdentifier();
  return url;
}

void RestartNavigation(LocalFrame* frame) {
  Document* document = frame->GetDocument();
  FrameLoadRequest request(document, ResourceRequest(document->Url()));
  request.SetClientRedirectReason(ClientNavigationReason::kReload);
  frame->Navigate(request, WebFrameLoadType::kReplaceCurrentItem);
}

}  // namespace

ApplicationCacheHostForFrame::ApplicationCacheHostForFrame(
    DocumentLoader* document_loader,
    mojom::blink::DocumentInterfaceBroker* interface_broker,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : ApplicationCacheHost(document_loader,
                           interface_broker,
                           std::move(task_runner)),
      local_frame_(document_loader->GetFrame()) {}

void ApplicationCacheHostForFrame::LogMessage(
    mojom::blink::ConsoleMessageLevel log_level,
    const String& message) {
  if (WebTestSupport::IsRunningWebTest())
    return;

  if (!local_frame_ || !local_frame_->IsMainFrame())
    return;

  Frame* main_frame = local_frame_->GetPage()->MainFrame();
  if (!main_frame->IsLocalFrame())
    return;
  // TODO(michaeln): Make app cache host per-frame and correctly report to the
  // involved frame.
  auto* local_frame = DynamicTo<LocalFrame>(main_frame);
  local_frame->GetDocument()->AddConsoleMessage(ConsoleMessage::Create(
      mojom::ConsoleMessageSource::kOther, log_level, message));
}

void ApplicationCacheHostForFrame::SetSubresourceFactory(
    network::mojom::blink::URLLoaderFactoryPtr url_loader_factory) {
  auto info = std::make_unique<URLLoaderFactoryBundleInfo>();
  info->appcache_factory_info().set_handle(
      url_loader_factory.PassInterface().PassHandle());
  local_frame_->Client()->UpdateSubresourceFactory(std::move(info));
}

void ApplicationCacheHostForFrame::WillStartLoadingMainResource(
    DocumentLoader* loader,
    const KURL& url,
    const String& method) {
  ApplicationCacheHost::WillStartLoadingMainResource(loader, url, method);
  if (!backend_host_.is_bound())
    return;

  original_main_resource_url_ = ClearUrlRef(url);
  is_get_method_ = (method == kHttpGETMethod);
  DCHECK(method == method.UpperASCII());

  const ApplicationCacheHost* spawning_host = nullptr;

  DCHECK(loader->GetFrame());
  LocalFrame* frame = loader->GetFrame();
  Frame* spawning_frame = frame->Tree().Parent();
  if (!spawning_frame || !IsA<LocalFrame>(spawning_frame))
    spawning_frame = frame->Loader().Opener();
  if (!spawning_frame || !IsA<LocalFrame>(spawning_frame))
    spawning_frame = frame;
  if (DocumentLoader* spawning_doc_loader =
          To<LocalFrame>(spawning_frame)->Loader().GetDocumentLoader()) {
    spawning_host = spawning_doc_loader->GetApplicationCacheHost();
  }

  if (spawning_host && (spawning_host != this) &&
      (spawning_host->GetStatus() !=
       mojom::blink::AppCacheStatus::APPCACHE_STATUS_UNCACHED)) {
    backend_host_->SetSpawningHostId(spawning_host->GetHostID());
  }
}

void ApplicationCacheHostForFrame::SelectCacheWithoutManifest() {
  if (!backend_host_.is_bound())
    return;

  if (was_select_cache_called_)
    return;
  was_select_cache_called_ = true;

  status_ =
      (document_response_.AppCacheID() == mojom::blink::kAppCacheNoCacheId)
          ? mojom::blink::AppCacheStatus::APPCACHE_STATUS_UNCACHED
          : mojom::blink::AppCacheStatus::APPCACHE_STATUS_CHECKING;
  is_new_master_entry_ = OLD_ENTRY;
  backend_host_->SelectCache(document_url_, document_response_.AppCacheID(),
                             KURL());
}

void ApplicationCacheHostForFrame::SelectCacheWithManifest(
    const KURL& manifest_url) {
  ApplicationCacheHost::SelectCacheWithManifest(manifest_url);

  if (!backend_host_.is_bound())
    return;

  if (was_select_cache_called_)
    return;
  was_select_cache_called_ = true;

  KURL manifest_kurl(ClearUrlRef(manifest_url));

  // 6.9.6 The application cache selection algorithm
  // Check for new 'master' entries.
  if (document_response_.AppCacheID() == mojom::blink::kAppCacheNoCacheId) {
    if (is_scheme_supported_ && is_get_method_ &&
        SecurityOrigin::AreSameSchemeHostPort(manifest_kurl, document_url_)) {
      status_ = mojom::blink::AppCacheStatus::APPCACHE_STATUS_CHECKING;
      is_new_master_entry_ = NEW_ENTRY;
    } else {
      status_ = mojom::blink::AppCacheStatus::APPCACHE_STATUS_UNCACHED;
      is_new_master_entry_ = OLD_ENTRY;
      manifest_kurl = KURL();
    }
    backend_host_->SelectCache(document_url_, mojom::blink::kAppCacheNoCacheId,
                               manifest_kurl);
    return;
  }

  DCHECK_EQ(OLD_ENTRY, is_new_master_entry_);

  // 6.9.6 The application cache selection algorithm
  // Check for 'foreign' entries.
  KURL document_manifest_kurl(document_response_.AppCacheManifestURL());
  if (document_manifest_kurl != manifest_kurl) {
    backend_host_->MarkAsForeignEntry(document_url_,
                                      document_response_.AppCacheID());
    status_ = mojom::blink::AppCacheStatus::APPCACHE_STATUS_UNCACHED;
    // It's a foreign entry, restart the current navigation from the top of the
    // navigation algorithm. The navigation will not result in the same resource
    // being loaded, because "foreign" entries are never picked during
    // navigation. see ApplicationCacheGroup::selectCache()
    RestartNavigation(local_frame_);  // the navigation will be restarted
    return;
  }

  status_ = mojom::blink::AppCacheStatus::APPCACHE_STATUS_CHECKING;

  // It's a 'master' entry that's already in the cache.
  backend_host_->SelectCache(document_url_, document_response_.AppCacheID(),
                             manifest_kurl);
}

void ApplicationCacheHostForFrame::DidReceiveResponseForMainResource(
    const ResourceResponse& response) {
  if (!backend_host_.is_bound())
    return;

  document_response_ = response;
  document_url_ = ClearUrlRef(document_response_.CurrentRequestUrl());
  if (document_url_ != original_main_resource_url_)
    is_get_method_ = true;  // A redirect was involved.
  original_main_resource_url_ = KURL();

  is_scheme_supported_ =
      Platform::Current()->IsURLSupportedForAppCache(document_url_);
  if ((document_response_.AppCacheID() != mojom::blink::kAppCacheNoCacheId) ||
      !is_scheme_supported_ || !is_get_method_)
    is_new_master_entry_ = OLD_ENTRY;
}

void ApplicationCacheHostForFrame::Trace(blink::Visitor* visitor) {
  visitor->Trace(local_frame_);
  ApplicationCacheHost::Trace(visitor);
}

}  // namespace blink
