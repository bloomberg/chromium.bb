// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_INCOGNITO_OBSERVER_H_
#define CHROME_BROWSER_METRICS_INCOGNITO_OBSERVER_H_

#include "base/callback.h"
#include "build/build_config.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

#if defined(OS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list_observer.h"
#endif

// Encapsulates platform-specific functionality for observing events that may
// cause "is incognito active?" state to change. The class takes a closure that
// will be called when an event happens that could result in a state change.
// The incognito state should then be checked by the callback.
// TODO(asvitkine): Considering moving the check for incognito to this class
// too; see ChromeMetricsServicesManagerClient::IsIncognitoSessionActive().
class IncognitoObserver :
#if defined(OS_ANDROID)
    public TabModelListObserver,
#endif
    content::NotificationObserver {
 public:
  explicit IncognitoObserver(const base::RepeatingClosure& update_closure);
  ~IncognitoObserver() override;

 private:
#if defined(OS_ANDROID)
  // TabModelListObserver:
  void OnTabModelAdded() override;
  void OnTabModelRemoved() override;
#endif

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  const base::RepeatingClosure update_closure_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(IncognitoObserver);
};

#endif  // CHROME_BROWSER_METRICS_INCOGNITO_OBSERVER_H_
