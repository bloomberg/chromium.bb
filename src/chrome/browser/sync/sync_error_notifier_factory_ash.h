// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNC_ERROR_NOTIFIER_FACTORY_ASH_H_
#define CHROME_BROWSER_SYNC_SYNC_ERROR_NOTIFIER_FACTORY_ASH_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class SyncErrorNotifier;
class Profile;

// Singleton that owns all SyncErrorNotifiers and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated SyncErrorNotifier.
class SyncErrorNotifierFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the instance of SyncErrorNotifier associated with this profile,
  // creating one if none exists and the shell exists.
  static SyncErrorNotifier* GetForProfile(Profile* profile);

  // Returns an instance of the SyncErrorNotifierFactory singleton.
  static SyncErrorNotifierFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<SyncErrorNotifierFactory>;

  SyncErrorNotifierFactory();
  ~SyncErrorNotifierFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;

  DISALLOW_COPY_AND_ASSIGN(SyncErrorNotifierFactory);
};

#endif  // CHROME_BROWSER_SYNC_SYNC_ERROR_NOTIFIER_FACTORY_ASH_H_
