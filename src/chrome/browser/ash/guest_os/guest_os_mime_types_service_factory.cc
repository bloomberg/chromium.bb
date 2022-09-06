// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_mime_types_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/guest_os/guest_os_mime_types_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace guest_os {

// static
GuestOsMimeTypesService* GuestOsMimeTypesServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<GuestOsMimeTypesService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
GuestOsMimeTypesServiceFactory* GuestOsMimeTypesServiceFactory::GetInstance() {
  static base::NoDestructor<GuestOsMimeTypesServiceFactory> factory;
  return factory.get();
}

GuestOsMimeTypesServiceFactory::GuestOsMimeTypesServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "GuestOsMimeTypesService",
          BrowserContextDependencyManager::GetInstance()) {}

GuestOsMimeTypesServiceFactory::~GuestOsMimeTypesServiceFactory() = default;

KeyedService* GuestOsMimeTypesServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new GuestOsMimeTypesService(profile);
}

}  // namespace guest_os
