// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bluetooth/bluetooth_chooser_context_factory.h"

#include "chrome/browser/bluetooth/bluetooth_chooser_context.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
BluetoothChooserContextFactory* BluetoothChooserContextFactory::GetInstance() {
  static base::NoDestructor<BluetoothChooserContextFactory> factory;
  return factory.get();
}

// static
BluetoothChooserContext* BluetoothChooserContextFactory::GetForProfile(
    Profile* profile) {
  return static_cast<BluetoothChooserContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

BluetoothChooserContextFactory::BluetoothChooserContextFactory()
    : BrowserContextKeyedServiceFactory(
          "BluetoothChooserContext",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

BluetoothChooserContextFactory::~BluetoothChooserContextFactory() = default;

KeyedService* BluetoothChooserContextFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new BluetoothChooserContext(Profile::FromBrowserContext(context));
}

content::BrowserContext* BluetoothChooserContextFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
