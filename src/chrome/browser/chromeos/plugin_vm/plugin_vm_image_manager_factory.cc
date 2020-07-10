// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_image_manager_factory.h"

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_image_manager.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace plugin_vm {

// static
PluginVmImageManager* PluginVmImageManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<PluginVmImageManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
PluginVmImageManagerFactory* PluginVmImageManagerFactory::GetInstance() {
  return base::Singleton<PluginVmImageManagerFactory>::get();
}

PluginVmImageManagerFactory::PluginVmImageManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "PluginVmImageManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(DownloadServiceFactory::GetInstance());
}

PluginVmImageManagerFactory::~PluginVmImageManagerFactory() = default;

KeyedService* PluginVmImageManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new PluginVmImageManager(Profile::FromBrowserContext(context));
}

content::BrowserContext* PluginVmImageManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

}  // namespace plugin_vm
