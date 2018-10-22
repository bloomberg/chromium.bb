// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/instant_unittest_base.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/google/core/browser/google_pref_names.h"
#include "components/google/core/browser/google_url_tracker.h"
#include "components/search/search.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/entropy_provider.h"

InstantUnitTestBase::InstantUnitTestBase() {
  field_trial_list_.reset(new base::FieldTrialList(
      std::make_unique<variations::SHA1EntropyProvider>("42")));
}

InstantUnitTestBase::~InstantUnitTestBase() {
}

void InstantUnitTestBase::SetUp() {
  BrowserWithTestWindowTest::SetUp();

  template_url_service_ = TemplateURLServiceFactory::GetForProfile(profile());
  search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service_);

  UIThreadSearchTermsData::SetGoogleBaseURL("https://www.google.com/");
  SetUserSelectedDefaultSearchProvider("{google:baseURL}");
  instant_service_ = InstantServiceFactory::GetForProfile(profile());
}

void InstantUnitTestBase::TearDown() {
  UIThreadSearchTermsData::SetGoogleBaseURL("");
  BrowserWithTestWindowTest::TearDown();
}

void InstantUnitTestBase::SetUserSelectedDefaultSearchProvider(
    const std::string& base_url) {
  TemplateURLData data;
  data.SetShortName(base::UTF8ToUTF16(base_url));
  data.SetKeyword(base::UTF8ToUTF16(base_url));
  data.SetURL(base_url + "url?bar={searchTerms}");
  data.new_tab_url = base_url + "newtab";
  data.alternate_urls.push_back(base_url + "alt#quux={searchTerms}");

  TemplateURL* template_url =
      template_url_service_->Add(std::make_unique<TemplateURL>(data));
  template_url_service_->SetUserSelectedDefaultSearchProvider(template_url);
}

void InstantUnitTestBase::NotifyGoogleBaseURLUpdate(
    const std::string& new_google_base_url) {
  // GoogleURLTracker is not created in tests.
  // (See GoogleURLTrackerFactory::ServiceIsNULLWhileTesting())
  // For determining google:baseURL for NTP, the following is used:
  // UIThreadSearchTermsData::GoogleBaseURLValue()
  // For simulating test behavior, this is overridden below.
  UIThreadSearchTermsData::SetGoogleBaseURL(new_google_base_url);
  TemplateURLServiceFactory::GetForProfile(profile())->GoogleBaseURLChanged();
}

TestingProfile* InstantUnitTestBase::CreateProfile() {
  TestingProfile* profile = BrowserWithTestWindowTest::CreateProfile();
  TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile, &TemplateURLServiceFactory::BuildInstanceFor);
  return profile;
}
