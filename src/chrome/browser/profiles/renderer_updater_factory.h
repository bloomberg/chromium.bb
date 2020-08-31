// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_RENDERER_UPDATER_FACTORY_H_
#define CHROME_BROWSER_PROFILES_RENDERER_UPDATER_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;
class RendererUpdater;

// Singleton that creates/deletes RendererUpdater as new Profiles are
// created/shutdown.
class RendererUpdaterFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns an instance of the RendererUpdaterFactory singleton.
  static RendererUpdaterFactory* GetInstance();

  // Returns the instance of RendererUpdater for the passed |profile|.
  static RendererUpdater* GetForProfile(Profile* profile);

 protected:
  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;

 private:
  friend struct base::DefaultSingletonTraits<RendererUpdaterFactory>;

  RendererUpdaterFactory();
  ~RendererUpdaterFactory() override;

  DISALLOW_COPY_AND_ASSIGN(RendererUpdaterFactory);
};

#endif  // CHROME_BROWSER_PROFILES_RENDERER_UPDATER_FACTORY_H_
