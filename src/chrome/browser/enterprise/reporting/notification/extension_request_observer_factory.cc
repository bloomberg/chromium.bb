// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/notification/extension_request_observer_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/reporting/notification/extension_request_observer.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace enterprise_reporting {

ExtensionRequestObserverFactory::ExtensionRequestObserverFactory(
    Profile* profile)
    : profile_(profile) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  profile_manager->AddObserver(this);

  if (profile) {
    OnProfileAdded(profile);
  } else {
    for (Profile* profile : profile_manager->GetLoadedProfiles())
      OnProfileAdded(profile);
  }
}

ExtensionRequestObserverFactory::~ExtensionRequestObserverFactory() {
  if (g_browser_process->profile_manager())
    g_browser_process->profile_manager()->RemoveObserver(this);

  if (profile_)
    profile_->RemoveObserver(this);
}

void ExtensionRequestObserverFactory::OnProfileAdded(Profile* profile) {
  if (profile->IsSystemProfile() || profile->IsGuestSession() ||
      profile->IsIncognitoProfile()) {
    return;
  }

  if (profile_ && (profile_ != profile || !observers_.empty()))
    return;

  if (profile_)
    profile->AddObserver(this);

  observers_.emplace(profile,
                     std::make_unique<ExtensionRequestObserver>(profile));
}

void ExtensionRequestObserverFactory::OnProfileMarkedForPermanentDeletion(
    Profile* profile) {
  if (profile_ && profile_ == profile)
    profile->RemoveObserver(this);

  observers_.erase(profile);
}

void ExtensionRequestObserverFactory::OnProfileWillBeDestroyed(
    Profile* profile) {
  DCHECK(profile_);

  if (profile_ == profile) {
    profile->RemoveObserver(this);
    observers_.erase(profile);
  }
}

ExtensionRequestObserver*
ExtensionRequestObserverFactory::GetObserverByProfileForTesting(
    Profile* profile) {
  auto it = observers_.find(profile);
  return it == observers_.end() ? nullptr : it->second.get();
}

int ExtensionRequestObserverFactory::GetNumberOfObserversForTesting() {
  return observers_.size();
}

}  // namespace enterprise_reporting
