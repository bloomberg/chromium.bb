// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/content_web_ui_controller_factory.h"

#include "build/build_config.h"
#include "content/browser/attribution_reporting/attribution_internals_ui.h"
#include "content/browser/gpu/gpu_internals_ui.h"
#include "content/browser/indexed_db/indexed_db_internals_ui.h"
#include "content/browser/media/media_internals_ui.h"
#include "content/browser/metrics/histograms_internals_ui.h"
#include "content/browser/net/network_errors_listing_ui.h"
#include "content/browser/prerender/prerender_internals_ui.h"
#include "content/browser/process_internals/process_internals_ui.h"
#include "content/browser/quota/quota_internals_ui.h"
#include "content/browser/service_worker/service_worker_internals_ui.h"
#include "content/browser/tracing/tracing_ui.h"
#include "content/browser/ukm_internals_ui.h"
#include "content/browser/webrtc/webrtc_internals_ui.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/url_constants.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"

namespace content {

WebUI::TypeID ContentWebUIControllerFactory::GetWebUIType(
    BrowserContext* browser_context,
    const GURL& url) {
  if (!url.SchemeIs(kChromeUIScheme))
    return WebUI::kNoWebUI;

  if (url.host_piece() == kChromeUIWebRTCInternalsHost ||
#if !BUILDFLAG(IS_ANDROID)
      url.host_piece() == kChromeUITracingHost ||
#endif
      url.host_piece() == kChromeUIGpuHost ||
      url.host_piece() == kChromeUIHistogramHost ||
      url.host_piece() == kChromeUIIndexedDBInternalsHost ||
      url.host_piece() == kChromeUIMediaInternalsHost ||
      url.host_piece() == kChromeUIServiceWorkerInternalsHost ||
      url.host_piece() == kChromeUINetworkErrorsListingHost ||
      url.host_piece() == kChromeUIPrerenderInternalsHost ||
      url.host_piece() == kChromeUIProcessInternalsHost ||
      url.host_piece() == kChromeUIAttributionInternalsHost ||
      url.host_piece() == kChromeUIQuotaInternals2Host ||
      url.host_piece() == kChromeUIUkmHost) {
    return const_cast<ContentWebUIControllerFactory*>(this);
  }
  return WebUI::kNoWebUI;
}

bool ContentWebUIControllerFactory::UseWebUIForURL(
    BrowserContext* browser_context,
    const GURL& url) {
  return GetWebUIType(browser_context, url) != WebUI::kNoWebUI;
}

std::unique_ptr<WebUIController>
ContentWebUIControllerFactory::CreateWebUIControllerForURL(WebUI* web_ui,
                                                           const GURL& url) {
  if (!url.SchemeIs(kChromeUIScheme))
    return nullptr;
  if (url.host_piece() == kChromeUIGpuHost)
    return std::make_unique<GpuInternalsUI>(web_ui);
  if (url.host_piece() == kChromeUIHistogramHost)
    return std::make_unique<HistogramsInternalsUI>(web_ui);
  if (url.host_piece() == kChromeUIIndexedDBInternalsHost)
    return std::make_unique<IndexedDBInternalsUI>(web_ui);
  if (url.host_piece() == kChromeUIServiceWorkerInternalsHost)
    return std::make_unique<ServiceWorkerInternalsUI>(web_ui);
  if (url.host_piece() == kChromeUINetworkErrorsListingHost)
    return std::make_unique<NetworkErrorsListingUI>(web_ui);
#if !BUILDFLAG(IS_ANDROID)
  if (url.host_piece() == kChromeUITracingHost)
    return std::make_unique<TracingUI>(web_ui);
#endif
  if (url.host_piece() == kChromeUIWebRTCInternalsHost)
    return std::make_unique<WebRTCInternalsUI>(web_ui);
  if (url.host_piece() == kChromeUIPrerenderInternalsHost)
    return std::make_unique<PrerenderInternalsUI>(web_ui);
  if (url.host_piece() == kChromeUIProcessInternalsHost)
    return std::make_unique<ProcessInternalsUI>(web_ui);
  if (url.host_piece() == kChromeUIAttributionInternalsHost)
    return std::make_unique<AttributionInternalsUI>(web_ui);
  if (url.host_piece() == kChromeUIQuotaInternals2Host)
    return std::make_unique<QuotaInternals2UI>(web_ui);
  if (url.host_piece() == kChromeUIUkmHost)
    return std::make_unique<UkmInternalsUI>(web_ui);
  if (url.host_piece() == kChromeUIMediaInternalsHost) {
    if (base::FeatureList::IsEnabled(media::kEnableMediaInternals))
      return std::make_unique<MediaInternalsUI>(web_ui);
    return nullptr;
  }
  return nullptr;
}

// static
ContentWebUIControllerFactory* ContentWebUIControllerFactory::GetInstance() {
  return base::Singleton<ContentWebUIControllerFactory>::get();
}

ContentWebUIControllerFactory::ContentWebUIControllerFactory() {
}

ContentWebUIControllerFactory::~ContentWebUIControllerFactory() {
}

}  // namespace content
