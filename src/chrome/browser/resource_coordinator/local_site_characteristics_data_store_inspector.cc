// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/local_site_characteristics_data_store_inspector.h"

#include "base/macros.h"
#include "chrome/browser/profiles/profile.h"

namespace resource_coordinator {

namespace {

const void* const kSiteCharacteristicsDataStoreInspectorUserKey =
    &kSiteCharacteristicsDataStoreInspectorUserKey;

class SiteCharacteristicsUserData : public base::SupportsUserData::Data {
 public:
  explicit SiteCharacteristicsUserData(
      LocalSiteCharacteristicsDataStoreInspector* inspector)
      : inspector_(inspector) {}

  LocalSiteCharacteristicsDataStoreInspector* inspector() const {
    return inspector_;
  }

 private:
  LocalSiteCharacteristicsDataStoreInspector* inspector_;
};

}  // namespace

// static
LocalSiteCharacteristicsDataStoreInspector*
LocalSiteCharacteristicsDataStoreInspector::GetForProfile(Profile* profile) {
  SiteCharacteristicsUserData* data = static_cast<SiteCharacteristicsUserData*>(
      profile->GetUserData(kSiteCharacteristicsDataStoreInspectorUserKey));

  if (!data)
    return nullptr;

  return data->inspector();
}

// static
void LocalSiteCharacteristicsDataStoreInspector::SetForProfile(
    LocalSiteCharacteristicsDataStoreInspector* inspector,
    Profile* profile) {
  if (inspector) {
    DCHECK_EQ(nullptr, GetForProfile(profile));

    profile->SetUserData(
        kSiteCharacteristicsDataStoreInspectorUserKey,
        std::make_unique<SiteCharacteristicsUserData>(inspector));
  } else {
    DCHECK_NE(nullptr, GetForProfile(profile));
    profile->RemoveUserData(kSiteCharacteristicsDataStoreInspectorUserKey);
  }
}

}  // namespace resource_coordinator
