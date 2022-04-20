// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/url_param_filter/cross_otr_observer_android.h"

namespace url_param_filter {
void MaybeCreateCrossOtrObserverForTabLaunchType(
    content::WebContents* web_contents,
    const TabModel::TabLaunchType type) {
  if (type == TabModel::TabLaunchType::FROM_LONGPRESS_INCOGNITO) {
    // Inherited from WebContentsUserData and checks for an already-attached
    // instance internally.
    CrossOtrObserver::CreateForWebContents(web_contents);
  }
}
}  // namespace url_param_filter
