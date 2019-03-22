// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_TAB_UTIL_H_
#define IOS_CHROME_BROWSER_TABS_TAB_UTIL_H_

#include "components/search_engines/template_url.h"
#import "ios/web/public/navigation_manager.h"
#include "ui/base/page_transition_types.h"

class GURL;

// Creates a WebLoadParams object for loading |url| with |transition_type|. If
// |post_data| is nonnull, the data and content-type of the post data will be
// added to the return value as well.
web::NavigationManager::WebLoadParams CreateWebLoadParams(
    const GURL& url,
    ui::PageTransition transition_type,
    TemplateURLRef::PostContent* post_data);

#endif  // IOS_CHROME_BROWSER_TABS_TAB_UTIL_H_
