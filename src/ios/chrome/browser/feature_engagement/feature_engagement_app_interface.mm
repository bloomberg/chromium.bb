// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/feature_engagement/feature_engagement_app_interface.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/singleton.h"
#import "base/test/ios/wait_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/feature_engagement/test/test_tracker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/feature_engagement/tracker_factory.h"
#import "ios/chrome/test/app/chrome_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

using base::test::ScopedFeatureList;
using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForActionTimeout;

namespace {
std::unique_ptr<KeyedService> CreateTestFeatureEngagementTracker(
    web::BrowserState*) {
  return feature_engagement::CreateTestTracker();
}

BOOL LoadFeatureEngagementTracker() {
  ChromeBrowserState* browser_state =
      chrome_test_util::GetOriginalBrowserState();

  feature_engagement::TrackerFactory::GetInstance()->SetTestingFactory(
      browser_state, base::BindRepeating(&CreateTestFeatureEngagementTracker));

  // Wait until the feature engagement tracker is initialized.
  return WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    return feature_engagement::TrackerFactory::GetForBrowserState(browser_state)
        ->IsInitialized();
  });
}

class ScopedFeatureListHolder {
 public:
  static ScopedFeatureListHolder* GetInstance() {
    return base::Singleton<ScopedFeatureListHolder>::get();
  }

  // Creates and returns new scoped feature list. List stays alive until
  // DestroyLists() is called. Allows to push multiple features via scoped
  // feature list as required by some FeatureEngagement tests.
  ScopedFeatureList& CreateList() {
    auto scoped_feature_list = std::make_unique<ScopedFeatureList>();
    scoped_feature_lists_.push_back(std::move(scoped_feature_list));
    return *(scoped_feature_lists_.back());
  }

  // Destroys all scoped feature lists objects created with CreateList().
  void DestroyLists() { scoped_feature_lists_.clear(); }

 private:
  ScopedFeatureListHolder() = default;
  std::vector<std::unique_ptr<ScopedFeatureList>> scoped_feature_lists_;
  friend struct base::DefaultSingletonTraits<ScopedFeatureListHolder>;

  DISALLOW_COPY_AND_ASSIGN(ScopedFeatureListHolder);
};

}  // namespace

@implementation FeatureEngagementAppInterface

+ (void)reset {
  ScopedFeatureListHolder::GetInstance()->DestroyLists();
}

+ (void)simulateChromeOpenedEvent {
  feature_engagement::TrackerFactory::GetForBrowserState(
      chrome_test_util::GetOriginalBrowserState())
      ->NotifyEvent(feature_engagement::events::kChromeOpened);
}

+ (BOOL)enableBadgedReadingListTriggering {
  std::map<std::string, std::string> badged_reading_list_params;

  badged_reading_list_params["event_1"] =
      "name:chrome_opened;comparator:>=5;window:90;storage:90";
  badged_reading_list_params["event_trigger"] =
      "name:badged_reading_list_trigger;comparator:==0;window:1095;storage:"
      "1095";
  badged_reading_list_params["event_used"] =
      "name:viewed_reading_list;comparator:==0;window:90;storage:90";
  badged_reading_list_params["session_rate"] = "==0";
  badged_reading_list_params["availability"] = "any";

  ScopedFeatureListHolder::GetInstance()
      ->CreateList()
      .InitAndEnableFeatureWithParameters(
          feature_engagement::kIPHBadgedReadingListFeature,
          badged_reading_list_params);
  return LoadFeatureEngagementTracker();
}

+ (BOOL)enableBadgedTranslateManualTrigger {
  std::map<std::string, std::string> badged_translate_manual_trigger_params;
  badged_translate_manual_trigger_params["availability"] = "any";
  badged_translate_manual_trigger_params["session_rate"] = "==0";
  badged_translate_manual_trigger_params["event_used"] =
      "name:triggered_translate_infobar;comparator:==0;window:360;storage:360";
  badged_translate_manual_trigger_params["event_trigger"] =
      "name:badged_translate_manual_trigger_trigger;comparator:==0;window:360;"
      "storage:360";

  ScopedFeatureListHolder::GetInstance()
      ->CreateList()
      .InitAndEnableFeatureWithParameters(
          feature_engagement::kIPHBadgedTranslateManualTriggerFeature,
          badged_translate_manual_trigger_params);
  return LoadFeatureEngagementTracker();
}

+ (BOOL)enableNewTabTipTriggering {
  std::map<std::string, std::string> new_tab_tip_params;

  new_tab_tip_params["event_1"] =
      "name:chrome_opened;comparator:>=3;window:90;storage:90";
  new_tab_tip_params["event_trigger"] =
      "name:new_tab_tip_trigger;comparator:<2;window:1095;storage:"
      "1095";
  new_tab_tip_params["event_used"] =
      "name:new_tab_opened;comparator:==0;window:90;storage:90";
  new_tab_tip_params["session_rate"] = "==0";
  new_tab_tip_params["availability"] = "any";

  ScopedFeatureListHolder::GetInstance()
      ->CreateList()
      .InitAndEnableFeatureWithParameters(
          feature_engagement::kIPHNewTabTipFeature, new_tab_tip_params);
  return LoadFeatureEngagementTracker();
}

+ (BOOL)enableBottomToolbarTipTriggering {
  std::map<std::string, std::string> bottom_toolbar_tip_params;

  bottom_toolbar_tip_params["availability"] = "any";
  bottom_toolbar_tip_params["session_rate"] = "==0";
  bottom_toolbar_tip_params["event_used"] =
      "name:bottom_toolbar_opened;comparator:any;window:90;storage:90";
  bottom_toolbar_tip_params["event_trigger"] =
      "name:bottom_toolbar_trigger;comparator:==0;window:90;storage:90";

  ScopedFeatureListHolder::GetInstance()
      ->CreateList()
      .InitAndEnableFeatureWithParameters(
          feature_engagement::kIPHBottomToolbarTipFeature,
          bottom_toolbar_tip_params);
  return LoadFeatureEngagementTracker();
}

+ (BOOL)enableLongPressTipTriggering {
  std::map<std::string, std::string> long_press_tip_params;

  long_press_tip_params["availability"] = "any";
  long_press_tip_params["session_rate"] = "<=1";
  long_press_tip_params["event_used"] =
      "name:long_press_toolbar_opened;comparator:any;window:90;storage:90";
  long_press_tip_params["event_trigger"] =
      "name:long_press_toolbar_trigger;comparator:==0;window:90;storage:90";
  long_press_tip_params["event_1"] =
      "name:bottom_toolbar_opened;comparator:>=1;window:90;storage:90";

  ScopedFeatureListHolder::GetInstance()
      ->CreateList()
      .InitAndEnableFeatureWithParameters(
          feature_engagement::kIPHLongPressToolbarTipFeature,
          long_press_tip_params);
  return LoadFeatureEngagementTracker();
}

+ (void)showTranslate {
  [chrome_test_util::BrowserCommandDispatcherForMainBVC() showTranslate];
}

+ (void)showReadingList {
  [chrome_test_util::BrowserCommandDispatcherForMainBVC() showReadingList];
}

@end
