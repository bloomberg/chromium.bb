// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/incognito_observer.h"

#include "chrome/browser/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"

IncognitoObserver::IncognitoObserver(
    const base::RepeatingClosure& update_closure)
    : update_closure_(update_closure) {
#if defined(OS_ANDROID)
  TabModelList::AddObserver(this);
#endif

  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_OPENED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

IncognitoObserver::~IncognitoObserver() {
#if defined(OS_ANDROID)
  TabModelList::RemoveObserver(this);
#endif
}

#if defined(OS_ANDROID)
void IncognitoObserver::OnTabModelAdded() {
  update_closure_.Run();
}

void IncognitoObserver::OnTabModelRemoved() {
  update_closure_.Run();
}
#endif

void IncognitoObserver::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_BROWSER_OPENED:
    case chrome::NOTIFICATION_PROFILE_DESTROYED:
      update_closure_.Run();
      break;

    default:
      NOTREACHED();
  }
}
