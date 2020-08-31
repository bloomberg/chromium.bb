// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feed_internals/feed_internals_ui.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "chrome/browser/android/feed/feed_host_service_factory.h"
#include "chrome/browser/android/feed/v2/feed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/feed_internals/feed_internals.mojom.h"
#include "chrome/browser/ui/webui/feed_internals/feed_internals_page_handler.h"
#include "chrome/browser/ui/webui/feed_internals/feedv2_internals_page_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/dev_ui_browser_resources.h"
#include "components/feed/feed_feature_list.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

FeedInternalsUI::FeedInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui), profile_(Profile::FromWebUI(web_ui)) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUISnippetsInternalsHost);

  source->AddResourcePath("feed_internals.js", IDR_FEED_INTERNALS_JS);
  source->AddResourcePath("feed_internals.mojom-lite.js",
                          IDR_FEED_INTERNALS_MOJO_JS);
  source->AddResourcePath("feed_internals.css", IDR_FEED_INTERNALS_CSS);
  source->SetDefaultResource(IDR_FEED_INTERNALS_HTML);

  content::WebUIDataSource::Add(profile_, source);
}

WEB_UI_CONTROLLER_TYPE_IMPL(FeedInternalsUI)

FeedInternalsUI::~FeedInternalsUI() = default;

void FeedInternalsUI::BindInterface(
    mojo::PendingReceiver<feed_internals::mojom::PageHandler> receiver) {
  if (!base::FeatureList::IsEnabled(feed::kInterestFeedV2)) {
    page_handler_ = std::make_unique<FeedInternalsPageHandler>(
        std::move(receiver),
        feed::FeedHostServiceFactory::GetForBrowserContext(profile_),
        profile_->GetPrefs());
  } else {
    v2_page_handler_ = std::make_unique<FeedV2InternalsPageHandler>(
        std::move(receiver),
        feed::FeedServiceFactory::GetForBrowserContext(profile_),
        profile_->GetPrefs());
  }
}
