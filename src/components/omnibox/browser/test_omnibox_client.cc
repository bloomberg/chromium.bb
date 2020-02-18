// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/test_omnibox_client.h"

#include <memory>
#include <utility>
#include <vector>

#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_scheme_classifier.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

TestOmniboxClient::TestOmniboxClient()
    : session_id_(SessionID::FromSerializedValue(1)),
      bookmark_model_(nullptr),
      autocomplete_classifier_(
          std::make_unique<AutocompleteController>(
              CreateAutocompleteProviderClient(),
              nullptr,
              AutocompleteClassifier::DefaultOmniboxProviders()),
          std::make_unique<TestSchemeClassifier>()) {}

TestOmniboxClient::~TestOmniboxClient() {
  autocomplete_classifier_.Shutdown();
}

std::unique_ptr<AutocompleteProviderClient>
TestOmniboxClient::CreateAutocompleteProviderClient() {
  std::unique_ptr<MockAutocompleteProviderClient> provider_client(
      new MockAutocompleteProviderClient());
  EXPECT_CALL(*provider_client, GetBuiltinURLs())
      .WillRepeatedly(testing::Return(std::vector<base::string16>()));
  EXPECT_CALL(*provider_client, GetSchemeClassifier())
      .WillRepeatedly(testing::ReturnRef(scheme_classifier_));

  std::unique_ptr<TemplateURLService> template_url_service(
      new TemplateURLService(
          nullptr /* PrefService */,
          std::unique_ptr<SearchTermsData>(new SearchTermsData),
          nullptr /* KeywordWebDataService */,
          std::unique_ptr<TemplateURLServiceClient>(), base::Closure()));

  // Save a reference to the created TemplateURLService for test use.
  template_url_service_ = template_url_service.get();

  provider_client->set_template_url_service(std::move(template_url_service));

  return std::move(provider_client);
}

std::unique_ptr<OmniboxNavigationObserver>
TestOmniboxClient::CreateOmniboxNavigationObserver(
    const base::string16& text,
    const AutocompleteMatch& match,
    const AutocompleteMatch& alternate_nav_match) {
  alternate_nav_match_ = alternate_nav_match;
  return nullptr;
}

bool TestOmniboxClient::IsPasteAndGoEnabled() const {
  return true;
}

const SessionID& TestOmniboxClient::GetSessionID() const {
  return session_id_;
}

void TestOmniboxClient::SetBookmarkModel(
    bookmarks::BookmarkModel* bookmark_model) {
  bookmark_model_ = bookmark_model;
}

bookmarks::BookmarkModel* TestOmniboxClient::GetBookmarkModel() {
  return bookmark_model_;
}

TemplateURLService* TestOmniboxClient::GetTemplateURLService() {
  DCHECK(template_url_service_);
  return template_url_service_;
}

const AutocompleteSchemeClassifier& TestOmniboxClient::GetSchemeClassifier()
    const {
  return scheme_classifier_;
}

AutocompleteClassifier* TestOmniboxClient::GetAutocompleteClassifier() {
  return &autocomplete_classifier_;
}

gfx::Image TestOmniboxClient::GetSizedIcon(
    const gfx::VectorIcon& vector_icon_type,
    SkColor vector_icon_color) const {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(16, 16);
  return gfx::Image(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
}

gfx::Image TestOmniboxClient::GetFaviconForPageUrl(
    const GURL& page_url,
    FaviconFetchedCallback on_favicon_fetched) {
  page_url_for_last_favicon_request_ = page_url;
  return gfx::Image();
}

GURL TestOmniboxClient::GetPageUrlForLastFaviconRequest() const {
  return page_url_for_last_favicon_request_;
}
