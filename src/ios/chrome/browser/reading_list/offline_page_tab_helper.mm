// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reading_list/offline_page_tab_helper.h"

#include "base/base64.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/post_task.h"
#include "base/timer/timer.h"
#include "components/reading_list/core/offline_url_utils.h"
#include "components/reading_list/core/reading_list_entry.h"
#include "components/reading_list/core/reading_list_model.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/reading_list/offline_url_utils.h"
#include "ios/chrome/browser/reading_list/reading_list_download_service.h"
#include "ios/chrome/browser/reading_list/reading_list_download_service_factory.h"
#import "ios/web/public/web_state/navigation_context.h"
#include "ios/web/public/web_task_traits.h"
#include "ios/web/public/web_thread.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Gets the offline data at |offline_path|. The result is a single std::string
// with all resources inlined.
// This method access file system and cannot be called on UI thread.
std::string GetOfflineData(base::FilePath offline_root,
                           base::FilePath offline_path) {
  base::FilePath absolute_path =
      reading_list::OfflineURLAbsolutePathFromRelativePath(offline_root,
                                                           offline_path);

  std::string content;
  if (!base::ReadFileToString(absolute_path, &content)) {
    return std::string();
  }

  if (offline_path.Extension() != ".html") {
    // If page is not html (in the current version, it means it is PDF), there
    // are no resources to inline.
    return content;
  }
  base::FileEnumerator images_enumerator(absolute_path.DirName(), false,
                                         base::FileEnumerator::FILES);
  for (base::FilePath image_path = images_enumerator.Next();
       !image_path.empty(); image_path = images_enumerator.Next()) {
    if (image_path == absolute_path) {
      // Skip the root file.
      continue;
    }
    std::string image;
    if (!base::ReadFileToString(image_path, &image)) {
      continue;
    }
    base::Base64Encode(image, &image);
    std::string src_with_file = base::StringPrintf(
        "src=\\\"%s\\\"", image_path.BaseName().value().c_str());
    std::string src_with_data = base::StringPrintf(
        "src=\\\"data:image/png;base64,%s\\\"", image.c_str());

    base::ReplaceSubstringsAfterOffset(&content, 0, src_with_file,
                                       src_with_data);
  }
  return content;
}
}

// static
void OfflinePageTabHelper::CreateForWebState(web::WebState* web_state,
                                             ReadingListModel* model) {
  if (!FromWebState(web_state)) {
    web_state->SetUserData(
        UserDataKey(),
        base::WrapUnique(new OfflinePageTabHelper(web_state, model)));
  }
}

OfflinePageTabHelper::OfflinePageTabHelper(web::WebState* web_state,
                                           ReadingListModel* model)
    : web_state_(web_state), reading_list_model_(model) {
  web_state_->AddObserver(this);
  reading_list_model_->AddObserver(this);
}

OfflinePageTabHelper::~OfflinePageTabHelper() {
  Detach();
}

void OfflinePageTabHelper::Detach() {
  StopCheckingLoadingProgress();
  if (reading_list_model_) {
    reading_list_model_->RemoveObserver(this);
    reading_list_model_ = nullptr;
  }
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
}

void OfflinePageTabHelper::LoadData(int offline_navigation,
                                    const GURL& url,
                                    const std::string& extension,
                                    const std::string& data) {
  if (!web_state_) {
    // It is possible that the web_state_ has been detached during the page
    // loading.
    return;
  }
  if (last_navigation_started_ != offline_navigation) {
    return;
  }

  NSString* mime = nil;
  if (extension == ".html") {
    mime = @"text/html";
  } else if (extension == ".pdf") {
    mime = @"application/pdf";
  }
  DCHECK(mime);
  presenting_offline_page_ = true;
  NSString* ns_data = base::SysUTF8ToNSString(data);
  offline_navigation_triggered_ = url;
  web_state_->LoadData([ns_data dataUsingEncoding:NSUTF8StringEncoding], mime,
                       url);
}

void OfflinePageTabHelper::DidStartNavigation(web::WebState* web_state,
                                              web::NavigationContext* context) {
  if (context->GetUrl() == offline_navigation_triggered_ ||
      context->IsSameDocument()) {
    // This is the navigation triggered by the loadData. Ignore it, to not reset
    // the presenting_offline_page_ flag.
    offline_navigation_triggered_ = GURL::EmptyGURL();
    return;
  }
  offline_navigation_triggered_ = GURL::EmptyGURL();
  if (!reading_list_model_->loaded()) {
    initial_navigation_url_ = GURL::EmptyGURL();
    return;
  }
  initial_navigation_url_ = context->GetUrl();
  loading_slow_or_failed_ = false;
  navigation_committed_ = false;
  last_navigation_started_++;
  bool is_reload = PageTransitionTypeIncludingQualifiersIs(
      context->GetPageTransition(), ui::PAGE_TRANSITION_RELOAD);
  if (!is_reload || !presenting_offline_page_) {
    StartCheckingLoadingProgress(initial_navigation_url_);
  }
  presenting_offline_page_ = false;
}

void OfflinePageTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->IsSameDocument()) {
    return;
  }
  navigation_committed_ = navigation_context->HasCommitted();
  if (!presenting_offline_page_) {
    PresentOfflinePageForOnlineUrl(initial_navigation_url_);
  }
}

GURL OfflinePageTabHelper::GetOnlineURLFromNavigationURL(
    const GURL& url) const {
  if (url.host() == kChromeUIOfflineHost) {
    return reading_list::EntryURLForOfflineURL(url);
  }
  return url;
}

void OfflinePageTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  StopCheckingLoadingProgress();
  // If the offline page was loaded directly, the initial_navigation_url_ is
  // chrome://offline?... and triggers a load error. Extract the meaningful
  // URL from the loading URL.
  GURL url = GetOnlineURLFromNavigationURL(initial_navigation_url_);

  if (load_completion_status == web::PageLoadCompletionStatus::FAILURE) {
    loading_slow_or_failed_ = true;
    PresentOfflinePageForOnlineUrl(url);
    return;
  }
  if (!url.is_valid() || !reading_list_model_->GetEntryByURL(url)) {
    return;
  }
  reading_list_model_->SetReadStatus(url, true);
  UMA_HISTOGRAM_BOOLEAN("ReadingList.OfflineVersionDisplayed",
                        presenting_offline_page_);
}

void OfflinePageTabHelper::WebStateDestroyed(web::WebState* web_state) {
  DCHECK(web_state_ == nullptr || web_state_ == web_state);
  Detach();
}

void OfflinePageTabHelper::ReadingListModelLoaded(
    const ReadingListModel* model) {
  // There is no need to do anything. If the navigation is in progress then
  // CheckLoadingProgress might still present distilled version of the page.
}

void OfflinePageTabHelper::ReadingListModelBeingDeleted(
    const ReadingListModel* model) {
  DCHECK(reading_list_model_ == nullptr || reading_list_model_ == model);
  Detach();

  // The call to RemoveUserData cause the destruction of the current instance,
  // so nothing should be done after that point (this is like "delete this;").
  // Unregistration as an observer happens in the destructor.
  web_state_->RemoveUserData(UserDataKey());
}

void OfflinePageTabHelper::PresentOfflinePageForOnlineUrl(const GURL& url) {
  if (!navigation_committed_ || !loading_slow_or_failed_) {
    return;
  }
  if (!HasDistilledVersionForOnlineUrl(url)) {
    return;
  }
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(web_state_->GetBrowserState());
  base::FilePath offline_root =
      ReadingListDownloadServiceFactory::GetForBrowserState(browser_state)
          ->OfflineRoot()
          .DirName();
  GURL entry_url = GetOnlineURLFromNavigationURL(url);
  const ReadingListEntry* entry = reading_list_model_->GetEntryByURL(entry_url);
  base::FilePath offline_path = entry->DistilledPath();
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&GetOfflineData, offline_root, offline_path),
      base::BindOnce(&OfflinePageTabHelper::LoadData, base::Unretained(this),
                     last_navigation_started_, entry_url,
                     offline_path.Extension()));
}

bool OfflinePageTabHelper::HasDistilledVersionForOnlineUrl(
    const GURL& online_url) const {
  if (!reading_list_model_ || !web_state_ || !reading_list_model_->loaded() ||
      !online_url.is_valid()) {
    return false;
  }

  GURL url = GetOnlineURLFromNavigationURL(online_url);
  const ReadingListEntry* entry = reading_list_model_->GetEntryByURL(url);
  if (!entry)
    return false;

  return entry->DistilledState() == ReadingListEntry::PROCESSED;
}

void OfflinePageTabHelper::StartCheckingLoadingProgress(const GURL& url) {
  if (reading_list_model_->loaded() && !HasDistilledVersionForOnlineUrl(url)) {
    // No need to launch the timer if there is no distilled version.
    return;
  }

  try_number_ = 0;
  timer_.reset(new base::RepeatingTimer());
  timer_->Start(FROM_HERE, base::TimeDelta::FromMilliseconds(1500),
                base::BindRepeating(&OfflinePageTabHelper::CheckLoadingProgress,
                                    base::Unretained(this), url));
}

void OfflinePageTabHelper::StopCheckingLoadingProgress() {
  timer_.reset();
}

void OfflinePageTabHelper::CheckLoadingProgress(const GURL& url) {
  if (reading_list_model_->loaded() && !HasDistilledVersionForOnlineUrl(url)) {
    StopCheckingLoadingProgress();
    return;
  }
  try_number_++;
  double progress = web_state_->GetLoadingProgress();
  const double kMinimumExpectedProgressPerStep = 0.25;
  if (progress < try_number_ * kMinimumExpectedProgressPerStep) {
    loading_slow_or_failed_ = true;
    PresentOfflinePageForOnlineUrl(url);
    StopCheckingLoadingProgress();
  } else if (try_number_ >= 3) {
    // Loading reached 75%, let the page finish normal loading.
    // Do not call StopCheckingLoadingProgress() as pending url is still needed
    // to mark the entry read on success loading or to display offline version
    // on error.
    timer_->Stop();
  }
}

WEB_STATE_USER_DATA_KEY_IMPL(OfflinePageTabHelper)
