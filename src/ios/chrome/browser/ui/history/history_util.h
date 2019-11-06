// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_UTIL_H_
#define IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_UTIL_H_

#import <Foundation/Foundation.h>

#include "base/strings/string16.h"

namespace base {
class Time;
}
class GURL;

namespace history {

// Returns a localized version of |visit_time| including a relative
// indicator (e.g. today, yesterday).
base::string16 GetRelativeDateLocalized(const base::Time& visit_time);

// Formats |title| to support RTL, or creates an RTL supported title based on
// |url| if |title| is empty.
NSString* FormattedTitle(const base::string16& title, const GURL& url);

}  // namespace history

#endif  // IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_UTIL_H_
