// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/translate_client_impl.h"

#include <memory>
#include <vector>

#include "components/infobars/core/infobar.h"
#include "components/language/core/browser/pref_names.h"
#include "components/translate/content/browser/content_translate_driver.h"
#include "components/translate/content/browser/content_translate_util.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/variations/service/variations_service.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/feature_list_creator.h"
#include "weblayer/browser/translate_accept_languages_factory.h"
#include "weblayer/browser/translate_ranker_factory.h"

namespace weblayer {

namespace {

std::unique_ptr<translate::TranslatePrefs> CreateTranslatePrefs(
    PrefService* prefs) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs(
      new translate::TranslatePrefs(prefs, language::prefs::kAcceptLanguages,
                                    /*preferred_languages_pref=*/nullptr));

  // We need to obtain the country here, since it comes from VariationsService.
  // components/ does not have access to that.
  variations::VariationsService* variations_service =
      FeatureListCreator::GetInstance()->variations_service();
  if (variations_service) {
    translate_prefs->SetCountry(
        variations_service->GetStoredPermanentCountry());
  }

  return translate_prefs;
}

}  // namespace

TranslateClientImpl::TranslateClientImpl(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      translate_driver_(&web_contents->GetController(),
                        /*url_language_histogram=*/nullptr),
      translate_manager_(new translate::TranslateManager(
          this,
          TranslateRankerFactory::GetForBrowserContext(
              web_contents->GetBrowserContext()),
          /*language_model=*/nullptr)) {
  translate_driver_.set_translate_manager(translate_manager_.get());
}

TranslateClientImpl::~TranslateClientImpl() = default;

translate::LanguageState& TranslateClientImpl::GetLanguageState() {
  return translate_manager_->GetLanguageState();
}

bool TranslateClientImpl::ShowTranslateUI(
    translate::TranslateStep step,
    const std::string& source_language,
    const std::string& target_language,
    translate::TranslateErrors::Type error_type,
    bool triggered_from_menu) {
  return false;
}

translate::TranslateDriver* TranslateClientImpl::GetTranslateDriver() {
  return &translate_driver_;
}

translate::TranslateManager* TranslateClientImpl::GetTranslateManager() {
  return translate_manager_.get();
}

PrefService* TranslateClientImpl::GetPrefs() {
  BrowserContextImpl* browser_context =
      static_cast<BrowserContextImpl*>(web_contents()->GetBrowserContext());
  return browser_context->pref_service();
}

std::unique_ptr<translate::TranslatePrefs>
TranslateClientImpl::GetTranslatePrefs() {
  return CreateTranslatePrefs(GetPrefs());
}

translate::TranslateAcceptLanguages*
TranslateClientImpl::GetTranslateAcceptLanguages() {
  return TranslateAcceptLanguagesFactory::GetForBrowserContext(
      web_contents()->GetBrowserContext());
}

#if defined(OS_ANDROID)
std::unique_ptr<infobars::InfoBar> TranslateClientImpl::CreateInfoBar(
    std::unique_ptr<translate::TranslateInfoBarDelegate> delegate) const {
  NOTREACHED();
  return nullptr;
}

int TranslateClientImpl::GetInfobarIconID() const {
  NOTREACHED();
  return 0;
}
#endif

bool TranslateClientImpl::IsTranslatableURL(const GURL& url) {
  return translate::IsTranslatableURL(url);
}

void TranslateClientImpl::ShowReportLanguageDetectionErrorUI(
    const GURL& report_url) {
  NOTREACHED();
}

void TranslateClientImpl::WebContentsDestroyed() {
  // Translation process can be interrupted.
  // Destroying the TranslateManager now guarantees that it never has to deal
  // with web_contents() being null.
  translate_manager_.reset();
}

}  // namespace weblayer

WEB_CONTENTS_USER_DATA_KEY_IMPL(weblayer::TranslateClientImpl)
