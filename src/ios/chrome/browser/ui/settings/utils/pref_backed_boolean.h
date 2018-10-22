// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_UTILS_PREF_BACKED_BOOLEAN_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_UTILS_PREF_BACKED_BOOLEAN_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/settings/utils/observable_boolean.h"

class PrefService;

// An observable boolean backed by a pref from a PrefService.
@interface PrefBackedBoolean : NSObject<ObservableBoolean>

// Returns a PrefBackedBoolean backed by |prefName| from |prefs|.
- (instancetype)initWithPrefService:(PrefService*)prefs
                           prefName:(const char*)prefName
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_UTILS_PREF_BACKED_BOOLEAN_H_
