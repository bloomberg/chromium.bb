// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_share_path_factory.h"

#include "chrome/browser/chromeos/crostini/crostini_share_path.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace crostini {

// static
CrostiniSharePath* CrostiniSharePathFactory::GetForProfile(Profile* profile) {
  return static_cast<CrostiniSharePath*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
CrostiniSharePathFactory* CrostiniSharePathFactory::GetInstance() {
  static base::NoDestructor<CrostiniSharePathFactory> factory;
  return factory.get();
}

CrostiniSharePathFactory::CrostiniSharePathFactory()
    : BrowserContextKeyedServiceFactory(
          "CrostiniSharePath",
          BrowserContextDependencyManager::GetInstance()) {}

CrostiniSharePathFactory::~CrostiniSharePathFactory() = default;

KeyedService* CrostiniSharePathFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new CrostiniSharePath(profile);
}

}  // namespace crostini
