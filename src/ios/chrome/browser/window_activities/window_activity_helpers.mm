// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/window_activities/window_activity_helpers.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#include "base/mac/foundation_util.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "net/base/mac/url_conversions.h"

// Activity types.
NSString* const kLoadURLActivityType = @"org.chromium.load.url";
NSString* const kLoadIncognitoURLActivityType = @"org.chromium.load.otr-url";

// User info keys.
NSString* const kURLKey = @"LoadParams_URL";
NSString* const kReferrerURLKey = @"LoadParams_ReferrerURL";
NSString* const kReferrerPolicyKey = @"LoadParams_ReferrerPolicy";

namespace {

// Helper for any actibity that opens URLs.
NSUserActivity* BaseActivityForURLOpening(bool in_incognito) {
  NSString* type =
      in_incognito ? kLoadIncognitoURLActivityType : kLoadURLActivityType;
  NSUserActivity* activity = [[NSUserActivity alloc] initWithActivityType:type];
  return activity;
}

}  // namespace

NSUserActivity* ActivityToOpenNewTab(bool in_incognito) {
  NSUserActivity* activity = BaseActivityForURLOpening(in_incognito);
  [activity addUserInfoEntriesFromDictionary:@{
    kURLKey : net::NSURLWithGURL(GURL(kChromeUINewTabURL))
  }];
  return activity;
}

NSUserActivity* ActivityToLoadURL(const GURL& url,
                                  const web::Referrer& referrer,
                                  bool in_incognito) {
  NSUserActivity* activity = BaseActivityForURLOpening(in_incognito);
  NSMutableDictionary* params = [[NSMutableDictionary alloc] init];
  if (!url.is_empty()) {
    params[kURLKey] = net::NSURLWithGURL(url);
  }
  if (!referrer.url.is_empty()) {
    params[kReferrerURLKey] = net::NSURLWithGURL(referrer.url);
    params[kReferrerPolicyKey] = @(static_cast<int>(referrer.policy));
  }
  [activity addUserInfoEntriesFromDictionary:params];
  return activity;
}

bool ActivityIsURLLoad(NSUserActivity* activity) {
  return [activity.activityType isEqualToString:kLoadURLActivityType] ||
         [activity.activityType isEqualToString:kLoadIncognitoURLActivityType];
}

UrlLoadParams LoadParamsFromActivity(NSUserActivity* activity) {
  if (!ActivityIsURLLoad(activity))
    return UrlLoadParams();

  bool incognito =
      [activity.activityType isEqualToString:kLoadIncognitoURLActivityType];
  NSURL* passed_url = base::mac::ObjCCast<NSURL>(activity.userInfo[kURLKey]);
  NSURL* referer_url =
      base::mac::ObjCCast<NSURL>(activity.userInfo[kReferrerURLKey]);

  GURL url = net::GURLWithNSURL(passed_url);
  UrlLoadParams params = UrlLoadParams::InNewTab(url);
  params.in_incognito = incognito;
  if (referer_url) {
    NSNumber* policy_value =
        base::mac::ObjCCast<NSNumber>(activity.userInfo[kReferrerPolicyKey]);
    web::ReferrerPolicy policy =
        static_cast<web::ReferrerPolicy>(policy_value.intValue);
    params.web_params.referrer =
        web::Referrer(net::GURLWithNSURL(referer_url), policy);
  }

  return params;
}
