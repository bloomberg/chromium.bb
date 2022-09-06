// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/translate/chrome_ios_translate_client.h"

#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "components/infobars/core/infobar.h"
#include "components/language/core/browser/accept_languages_service.h"
#include "components/language/core/browser/language_model_manager.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/translate/core/browser/page_translated_details.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_metrics_logger_impl.h"
#include "components/translate/core/browser/translate_step.h"
#include "components/translate/core/common/language_detection_details.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#include "ios/chrome/browser/language/accept_languages_service_factory.h"
#include "ios/chrome/browser/language/language_model_manager_factory.h"
#include "ios/chrome/browser/translate/language_detection_model_service_factory.h"
#include "ios/chrome/browser/translate/translate_model_service_factory.h"
#include "ios/chrome/browser/translate/translate_ranker_factory.h"
#include "ios/chrome/browser/translate/translate_service_ios.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"
#include "third_party/metrics_proto/translate_event.pb.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
void ChromeIOSTranslateClient::CreateForWebState(web::WebState* web_state) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(
        UserDataKey(),
        base::WrapUnique(new ChromeIOSTranslateClient(web_state)));
  }
}

ChromeIOSTranslateClient::ChromeIOSTranslateClient(web::WebState* web_state)
    : web_state_(web_state),
      translate_manager_(std::make_unique<translate::TranslateManager>(
          this,
          translate::TranslateRankerFactory::GetForBrowserState(
              ChromeBrowserState::FromBrowserState(
                  web_state->GetBrowserState())),
          LanguageModelManagerFactory::GetForBrowserState(
              ChromeBrowserState::FromBrowserState(
                  web_state->GetBrowserState()))
              ->GetPrimaryModel())),
      translate_driver_(
          web_state,
          translate_manager_.get(),
          LanguageDetectionModelServiceFactory::GetForBrowserState(
              ChromeBrowserState::FromBrowserState(
                  web_state->GetBrowserState()))) {
  web_state_->AddObserver(this);
}

ChromeIOSTranslateClient::~ChromeIOSTranslateClient() {
  DCHECK(!web_state_);
}

// static
std::unique_ptr<translate::TranslatePrefs>
ChromeIOSTranslateClient::CreateTranslatePrefs(PrefService* prefs) {
  return std::unique_ptr<translate::TranslatePrefs>(
      new translate::TranslatePrefs(prefs));
}

translate::TranslateManager* ChromeIOSTranslateClient::GetTranslateManager() {
  return translate_manager_.get();
}

// TranslateClient implementation:

std::unique_ptr<infobars::InfoBar> ChromeIOSTranslateClient::CreateInfoBar(
    std::unique_ptr<translate::TranslateInfoBarDelegate> delegate) const {
  bool skip_banner = delegate->translate_step() ==
                     translate::TranslateStep::TRANSLATE_STEP_TRANSLATING;
    return std::make_unique<InfoBarIOS>(InfobarType::kInfobarTypeTranslate,
                                        std::move(delegate), skip_banner);
}

bool ChromeIOSTranslateClient::ShowTranslateUI(
    translate::TranslateStep step,
    const std::string& source_language,
    const std::string& target_language,
    translate::TranslateErrors::Type error_type,
    bool triggered_from_menu) {
  DCHECK(web_state_);
  if (error_type != translate::TranslateErrors::NONE)
    step = translate::TRANSLATE_STEP_TRANSLATE_ERROR;

  // Infobar UI.
  translate::TranslateInfoBarDelegate::Create(
      step != translate::TRANSLATE_STEP_BEFORE_TRANSLATE,
      translate_manager_->GetWeakPtr(),
      InfoBarManagerImpl::FromWebState(web_state_),
      web_state_->GetBrowserState()->IsOffTheRecord(), step, source_language,
      target_language, error_type, triggered_from_menu);

  return true;
}

translate::IOSTranslateDriver* ChromeIOSTranslateClient::GetTranslateDriver() {
  return &translate_driver_;
}

PrefService* ChromeIOSTranslateClient::GetPrefs() {
  DCHECK(web_state_);
  ChromeBrowserState* chrome_browser_state =
      ChromeBrowserState::FromBrowserState(web_state_->GetBrowserState());
  return chrome_browser_state->GetOriginalChromeBrowserState()->GetPrefs();
}

std::unique_ptr<translate::TranslatePrefs>
ChromeIOSTranslateClient::GetTranslatePrefs() {
  DCHECK(web_state_);
  ChromeBrowserState* chrome_browser_state =
      ChromeBrowserState::FromBrowserState(web_state_->GetBrowserState());
  return CreateTranslatePrefs(chrome_browser_state->GetPrefs());
}

language::AcceptLanguagesService*
ChromeIOSTranslateClient::GetAcceptLanguagesService() {
  DCHECK(web_state_);
  return AcceptLanguagesServiceFactory::GetForBrowserState(
      ChromeBrowserState::FromBrowserState(web_state_->GetBrowserState()));
}

int ChromeIOSTranslateClient::GetInfobarIconID() const {
  return IDR_IOS_INFOBAR_TRANSLATE;
}

bool ChromeIOSTranslateClient::IsTranslatableURL(const GURL& url) {
  return TranslateServiceIOS::IsTranslatableURL(url);
}

bool ChromeIOSTranslateClient::IsAutofillAssistantRunning() const {
  return false;
}

void ChromeIOSTranslateClient::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->IsSameDocument())
    return;

  DidPageLoadComplete();
  // Lifetime of TranslateMetricsLogger should be each page load. So, we need to
  // detect the page load completion, i.e. the tab was closed, new navigation
  // replaced the page load, etc, and clear the logger.
  translate_metrics_logger_ =
      std::make_unique<translate::TranslateMetricsLoggerImpl>(
          translate_manager_->GetWeakPtr());
  translate_metrics_logger_->OnPageLoadStart(web_state->IsVisible());
}

void ChromeIOSTranslateClient::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->GetError()) {
    translate_metrics_logger_.reset();
    return;
  }

  if (!navigation_context->IsSameDocument() && translate_metrics_logger_) {
    translate_metrics_logger_->SetUkmSourceId(
        translate_driver_.GetUkmSourceId());
  }
}

void ChromeIOSTranslateClient::WasShown(web::WebState* web_state) {
  if (translate_metrics_logger_)
    translate_metrics_logger_->OnForegroundChange(true);
};

void ChromeIOSTranslateClient::WasHidden(web::WebState* web_state) {
  if (translate_metrics_logger_)
    translate_metrics_logger_->OnForegroundChange(false);
};

void ChromeIOSTranslateClient::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;

  DidPageLoadComplete();

  // Translation process can be interrupted.
  // Destroying the TranslateManager now guarantees that it never has to deal
  // with nullptr WebState.
  translate_manager_.reset();
}

void ChromeIOSTranslateClient::DidPageLoadComplete() {
  if (translate_metrics_logger_) {
    translate_metrics_logger_->RecordMetrics(true);
    translate_metrics_logger_.reset();
  }
}

WEB_STATE_USER_DATA_KEY_IMPL(ChromeIOSTranslateClient)
