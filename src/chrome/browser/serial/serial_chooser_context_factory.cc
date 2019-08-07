// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/serial/serial_chooser_context_factory.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/serial/serial_chooser_context.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

SerialChooserContextFactory::SerialChooserContextFactory()
    : BrowserContextKeyedServiceFactory(
          "SerialChooserContext",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

SerialChooserContextFactory::~SerialChooserContextFactory() {}

KeyedService* SerialChooserContextFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new SerialChooserContext(Profile::FromBrowserContext(context));
}

// static
SerialChooserContextFactory* SerialChooserContextFactory::GetInstance() {
  return base::Singleton<SerialChooserContextFactory>::get();
}

// static
SerialChooserContext* SerialChooserContextFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SerialChooserContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

content::BrowserContext* SerialChooserContextFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
