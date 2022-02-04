// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change_success_tracker_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/password_manager/core/browser/password_change_success_tracker_impl.h"
#include "content/public/browser/browser_context.h"

PasswordChangeSuccessTrackerFactory::PasswordChangeSuccessTrackerFactory()
    : BrowserContextKeyedServiceFactory(
          "PasswordChangeSuccessTracker",
          BrowserContextDependencyManager::GetInstance()) {}

PasswordChangeSuccessTrackerFactory::~PasswordChangeSuccessTrackerFactory() =
    default;

// static
PasswordChangeSuccessTrackerFactory*
PasswordChangeSuccessTrackerFactory::GetInstance() {
  static base::NoDestructor<PasswordChangeSuccessTrackerFactory> instance;
  return instance.get();
}

// static
password_manager::PasswordChangeSuccessTracker*
PasswordChangeSuccessTrackerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<password_manager::PasswordChangeSuccessTracker*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

KeyedService* PasswordChangeSuccessTrackerFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  return new password_manager::PasswordChangeSuccessTrackerImpl();
}
