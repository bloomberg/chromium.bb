// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/translate/chrome_ios_translate_client.h"

#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/infobars/core/infobar.h"
#include "components/language/core/browser/language_model_manager.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/translate/core/browser/page_translated_details.h"
#include "components/translate/core/browser/translate_accept_languages.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_step.h"
#include "components/translate/core/common/language_detection_details.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/infobars/infobar_controller.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#include "ios/chrome/browser/language/language_model_manager_factory.h"
#import "ios/chrome/browser/translate/language_selection_handler.h"
#include "ios/chrome/browser/translate/translate_accept_languages_factory.h"
#import "ios/chrome/browser/translate/translate_infobar_controller.h"
#import "ios/chrome/browser/translate/translate_option_selection_handler.h"
#include "ios/chrome/browser/translate/translate_ranker_factory.h"
#include "ios/chrome/browser/translate/translate_service_ios.h"
#import "ios/chrome/browser/ui/infobars/coordinators/infobar_translate_coordinator.h"
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"
#import "ios/chrome/browser/ui/translate/translate_notification_handler.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/navigation/navigation_item.h"
#include "ios/web/public/navigation/navigation_manager.h"
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
              ios::ChromeBrowserState::FromBrowserState(
                  web_state->GetBrowserState())),
          LanguageModelManagerFactory::GetForBrowserState(
              ios::ChromeBrowserState::FromBrowserState(
                  web_state->GetBrowserState()))
              ->GetPrimaryModel())),
      translate_driver_(web_state,
                        web_state->GetNavigationManager(),
                        translate_manager_.get()) {
  web_state_->AddObserver(this);
}

ChromeIOSTranslateClient::~ChromeIOSTranslateClient() {
  DCHECK(!web_state_);
}

// static
std::unique_ptr<translate::TranslatePrefs>
ChromeIOSTranslateClient::CreateTranslatePrefs(PrefService* prefs) {
  return std::unique_ptr<translate::TranslatePrefs>(
      new translate::TranslatePrefs(prefs, language::prefs::kAcceptLanguages,
                                    nullptr));
}

translate::TranslateManager* ChromeIOSTranslateClient::GetTranslateManager() {
  return translate_manager_.get();
}

// TranslateClient implementation:

std::unique_ptr<infobars::InfoBar> ChromeIOSTranslateClient::CreateInfoBar(
    std::unique_ptr<translate::TranslateInfoBarDelegate> delegate) const {
  if (IsTranslateInfobarMessagesUIEnabled()) {
    TranslateInfobarCoordinator* coordinator =
        [[TranslateInfobarCoordinator alloc]
            initWithInfoBarDelegate:delegate.get()];
    return std::make_unique<InfoBarIOS>(coordinator, std::move(delegate));
  } else {
    TranslateInfoBarController* controller = [[TranslateInfoBarController alloc]
        initWithInfoBarDelegate:delegate.get()];
    controller.languageSelectionHandler = language_selection_handler_;
    controller.translateOptionSelectionHandler =
        translate_option_selection_handler_;
    controller.translateNotificationHandler = translate_notification_handler_;
    return std::make_unique<InfoBarIOS>(controller, std::move(delegate));
  }
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
  ios::ChromeBrowserState* chrome_browser_state =
      ios::ChromeBrowserState::FromBrowserState(web_state_->GetBrowserState());
  return chrome_browser_state->GetOriginalChromeBrowserState()->GetPrefs();
}

std::unique_ptr<translate::TranslatePrefs>
ChromeIOSTranslateClient::GetTranslatePrefs() {
  DCHECK(web_state_);
  ios::ChromeBrowserState* chrome_browser_state =
      ios::ChromeBrowserState::FromBrowserState(web_state_->GetBrowserState());
  return CreateTranslatePrefs(chrome_browser_state->GetPrefs());
}

translate::TranslateAcceptLanguages*
ChromeIOSTranslateClient::GetTranslateAcceptLanguages() {
  DCHECK(web_state_);
  return TranslateAcceptLanguagesFactory::GetForBrowserState(
      ios::ChromeBrowserState::FromBrowserState(web_state_->GetBrowserState()));
}

int ChromeIOSTranslateClient::GetInfobarIconID() const {
  return IDR_IOS_INFOBAR_TRANSLATE;
}

bool ChromeIOSTranslateClient::IsTranslatableURL(const GURL& url) {
  return TranslateServiceIOS::IsTranslatableURL(url);
}

void ChromeIOSTranslateClient::ShowReportLanguageDetectionErrorUI(
    const GURL& report_url) {
  NOTREACHED();
}

void ChromeIOSTranslateClient::DidStartLoading(web::WebState* web_state) {
  [language_selection_handler_ dismissLanguageSelector];
  [translate_option_selection_handler_ dismissTranslateOptionSelector];
  [translate_notification_handler_ dismissNotification];
}

void ChromeIOSTranslateClient::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;

  [language_selection_handler_ dismissLanguageSelector];
  [translate_option_selection_handler_ dismissTranslateOptionSelector];
  [translate_notification_handler_ dismissNotification];

  // Translation process can be interrupted.
  // Destroying the TranslateManager now guarantees that it never has to deal
  // with nullptr WebState.
  translate_manager_.reset();
}

WEB_STATE_USER_DATA_KEY_IMPL(ChromeIOSTranslateClient)
