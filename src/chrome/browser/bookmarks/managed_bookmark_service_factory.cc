// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"

#include <string>

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/managed_ui.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/policy_constants.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace {

std::unique_ptr<KeyedService> BuildManagedBookmarkService(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<bookmarks::ManagedBookmarkService>(
      profile->GetPrefs(),
      base::BindRepeating(
          &ManagedBookmarkServiceFactory::GetManagedBookmarksManager,
          base::Unretained(profile)));
}

}  // namespace

// static
bookmarks::ManagedBookmarkService* ManagedBookmarkServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<bookmarks::ManagedBookmarkService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ManagedBookmarkServiceFactory* ManagedBookmarkServiceFactory::GetInstance() {
  return base::Singleton<ManagedBookmarkServiceFactory>::get();
}

// static
BrowserContextKeyedServiceFactory::TestingFactory
ManagedBookmarkServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildManagedBookmarkService);
}

// static
std::string ManagedBookmarkServiceFactory::GetManagedBookmarksManager(
    Profile* profile) {
  policy::ProfilePolicyConnector* connector =
      profile->GetProfilePolicyConnector();
  if (connector->IsManaged() &&
      connector->IsProfilePolicy(policy::key::kManagedBookmarks)) {
    absl::optional<std::string> account_manager =
        chrome::GetAccountManagerIdentity(profile);
    if (account_manager)
      return *account_manager;
  }
  return std::string();
}

ManagedBookmarkServiceFactory::ManagedBookmarkServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ManagedBookmarkService",
          BrowserContextDependencyManager::GetInstance()) {}

ManagedBookmarkServiceFactory::~ManagedBookmarkServiceFactory() {}

KeyedService* ManagedBookmarkServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return BuildManagedBookmarkService(context).release();
}

content::BrowserContext* ManagedBookmarkServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool ManagedBookmarkServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
