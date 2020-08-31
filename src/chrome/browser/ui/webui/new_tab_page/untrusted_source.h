// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_UNTRUSTED_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_UNTRUSTED_SOURCE_H_

#include <string>
#include <vector>

#include "base/scoped_observer.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_service.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_service_observer.h"
#include "chrome/browser/search/promos/promo_service.h"
#include "chrome/browser/search/promos/promo_service_observer.h"
#include "content/public/browser/url_data_source.h"

class Profile;

// Serves chrome-untrusted://new-tab-page/* sources which can return content
// from outside the chromium codebase. The chrome-untrusted://new-tab-page/*
// sources can only be embedded in the chrome://new-tab-page by using an
// <iframe>.
//
// Offers the following helpers to embed content into chrome://new-tab-page in a
// generalized way:
//   * chrome-untrusted://new-tab-page/image?<url>: Behaves like an img element
//       with src set to <url>.
//   * chrome-untrusted://new-tab-page/background_image?<url>: Behaves like an
//       element that has <url> set as the background image, such that the image
//       will cover the entire element.
//   * chrome-untrusted://new-tab-page/custom_background_image?<params>: Similar
//       to background_image but allows for custom styling. <params> are of the
//       form <key>=<value>. The following keys are supported:
//         * url:       background image URL.
//         * url2x:     (optional) URL to a higher res background image.
//         * size:      (optional) CSS background-size property.
//         * repeatX:   (optional) CSS background-repeat-x property.
//         * repeatY:   (optional) CSS background-repeat-y property.
//         * positionX: (optional) CSS background-position-x property.
//         * positionY: (optional) CSS background-position-y property.
//   * chrome-untrusted://new-tab-page/iframe?<url>: Behaves like an iframe with
//       src set to <url>.
//   Each of those helpers only accept URLs with HTTPS or chrome-untrusted:.
class UntrustedSource : public content::URLDataSource,
                        public OneGoogleBarServiceObserver,
                        public PromoServiceObserver {
 public:
  explicit UntrustedSource(Profile* profile);
  ~UntrustedSource() override;
  UntrustedSource(const UntrustedSource&) = delete;
  UntrustedSource& operator=(const UntrustedSource&) = delete;

  // content::URLDataSource:
  std::string GetContentSecurityPolicyScriptSrc() override;
  std::string GetContentSecurityPolicyChildSrc() override;
  std::string GetSource() override;
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override;
  std::string GetMimeType(const std::string& path) override;
  bool AllowCaching() override;
  std::string GetContentSecurityPolicyFrameAncestors() override;
  bool ShouldReplaceExistingSource() override;
  bool ShouldServiceRequest(const GURL& url,
                            content::BrowserContext* browser_context,
                            int render_process_id) override;

 private:
  // OneGoogleBarServiceObserver:
  void OnOneGoogleBarDataUpdated() override;
  void OnOneGoogleBarServiceShuttingDown() override;

  // PromoServiceObserver:
  void OnPromoDataUpdated() override;
  void OnPromoServiceShuttingDown() override;

  void ServeBackgroundImage(const GURL& url,
                            const GURL& url_2x,
                            const std::string& size,
                            const std::string& repeat_x,
                            const std::string& repeat_y,
                            const std::string& position_x,
                            const std::string& position_y,
                            content::URLDataSource::GotDataCallback callback);

  std::vector<content::URLDataSource::GotDataCallback>
      one_google_bar_callbacks_;
  OneGoogleBarService* one_google_bar_service_;
  ScopedObserver<OneGoogleBarService, OneGoogleBarServiceObserver>
      one_google_bar_service_observer_{this};
  base::Optional<base::TimeTicks> one_google_bar_load_start_time_;
  Profile* profile_;
  std::vector<content::URLDataSource::GotDataCallback> promo_callbacks_;
  PromoService* promo_service_;
  ScopedObserver<PromoService, PromoServiceObserver> promo_service_observer_{
      this};
  base::Optional<base::TimeTicks> promo_load_start_time_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_UNTRUSTED_SOURCE_H_
