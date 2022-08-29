// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_UPDATE_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_UPDATE_H_

#include <ostream>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "components/services/app_service/public/cpp/shortcut.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace apps {

class AppRegistryCacheTest;
struct IconKey;
struct RunOnOsLogin;

// Wraps two apps::mojom::AppPtr's, a prior state and a delta on top of that
// state. The state is conceptually the "sum" of all of the previous deltas,
// with "addition" or "merging" simply being that the most recent version of
// each field "wins".
//
// The state may be nullptr, meaning that there are no previous deltas.
// Alternatively, the delta may be nullptr, meaning that there is no change in
// state. At least one of state and delta must be non-nullptr.
//
// Almost all of an AppPtr's fields are optional. For example, if an app's name
// hasn't changed, then a delta doesn't necessarily have to contain a copy of
// the name, as the prior state should already contain it.
//
// The combination of the two (state and delta) can answer questions such as:
//  - What is the app's name? If the delta knows, that's the answer. Otherwise,
//    ask the state.
//  - Is the app ready to launch (i.e. installed)? Likewise, if the delta says
//    yes or no, that's the answer. Otherwise, the delta says "unknown", so ask
//    the state.
//  - Was the app *freshly* installed (i.e. it previously wasn't installed, but
//    now is)? Has its readiness changed? Check if the delta says "installed"
//    and the state says either "uninstalled" or unknown.
//
// An AppUpdate is read-only once constructed. All of its fields and methods
// are const. The constructor caller must guarantee that the AppPtr references
// remain valid for the lifetime of the AppUpdate.
//
// See components/services/app_service/README.md for more details.
//
// TODO(crbug.com/1253250): Remove all apps::mojom related code.
// 1. Modify comments.
// 2. Replace mojom related functions with non-mojom functions.
class COMPONENT_EXPORT(APP_UPDATE) AppUpdate {
 public:
  // Modifies |state| by copying over all of |delta|'s known fields: those
  // fields whose values aren't "unknown". The |state| may not be nullptr.
  static void Merge(apps::mojom::App* state, const apps::mojom::App* delta);
  static void Merge(App* state, const App* delta);

  // At most one of |state| or |delta| may be nullptr.
  AppUpdate(const apps::mojom::App* state,
            const apps::mojom::App* delta,
            const AccountId& account_id);
  AppUpdate(const App* state, const App* delta, const AccountId& account_id);

  AppUpdate(const AppUpdate&) = delete;
  AppUpdate& operator=(const AppUpdate&) = delete;

  // Returns whether this is the first update for the given AppId.
  // Equivalently, there are no previous deltas for the AppId.
  bool StateIsNull() const;

  apps::AppType AppType() const;

  const std::string& AppId() const;

  apps::Readiness Readiness() const;
  apps::Readiness PriorReadiness() const;
  bool ReadinessChanged() const;

  const std::string& Name() const;
  bool NameChanged() const;

  const std::string& ShortName() const;
  bool ShortNameChanged() const;

  // The publisher-specific ID for this app, e.g. for Android apps, this field
  // contains the Android package name. May be empty if AppId() should be
  // considered as the canonical publisher ID.
  const std::string& PublisherId() const;
  bool PublisherIdChanged() const;

  const std::string& Description() const;
  bool DescriptionChanged() const;

  const std::string& Version() const;
  bool VersionChanged() const;

  std::vector<std::string> AdditionalSearchTerms() const;
  bool AdditionalSearchTermsChanged() const;

  absl::optional<apps::IconKey> IconKey() const;
  bool IconKeyChanged() const;

  base::Time LastLaunchTime() const;
  bool LastLaunchTimeChanged() const;

  base::Time InstallTime() const;
  bool InstallTimeChanged() const;

  apps::Permissions Permissions() const;
  bool PermissionsChanged() const;

  apps::InstallReason InstallReason() const;
  bool InstallReasonChanged() const;

  apps::InstallSource InstallSource() const;
  bool InstallSourceChanged() const;

  // An optional ID used for policy to identify the app.
  // For web apps, it contains the install URL.
  const std::string& PolicyId() const;
  bool PolicyIdChanged() const;

  bool InstalledInternally() const;

  absl::optional<bool> IsPlatformApp() const;
  bool IsPlatformAppChanged() const;

  absl::optional<bool> Recommendable() const;
  bool RecommendableChanged() const;

  absl::optional<bool> Searchable() const;
  bool SearchableChanged() const;

  absl::optional<bool> ShowInLauncher() const;
  bool ShowInLauncherChanged() const;

  absl::optional<bool> ShowInShelf() const;
  bool ShowInShelfChanged() const;

  absl::optional<bool> ShowInSearch() const;
  bool ShowInSearchChanged() const;

  absl::optional<bool> ShowInManagement() const;
  bool ShowInManagementChanged() const;

  absl::optional<bool> HandlesIntents() const;
  bool HandlesIntentsChanged() const;

  absl::optional<bool> AllowUninstall() const;
  bool AllowUninstallChanged() const;

  absl::optional<bool> HasBadge() const;
  bool HasBadgeChanged() const;

  absl::optional<bool> Paused() const;
  bool PausedChanged() const;

  apps::IntentFilters IntentFilters() const;
  bool IntentFiltersChanged() const;

  absl::optional<bool> ResizeLocked() const;
  bool ResizeLockedChanged() const;

  apps::WindowMode WindowMode() const;
  bool WindowModeChanged() const;

  absl::optional<apps::RunOnOsLogin> RunOnOsLogin() const;
  bool RunOnOsLoginChanged() const;

  apps::Shortcuts Shortcuts() const;
  bool ShortcutsChanged() const;

  const ::AccountId& AccountId() const;

  absl::optional<uint64_t> AppSizeInBytes() const;
  bool AppSizeInBytesChanged() const;

  absl::optional<uint64_t> DataSizeInBytes() const;
  bool DataSizeInBytesChanged() const;

 private:
  friend class AppRegistryCacheTest;

  bool ShouldUseNonMojom() const;

  raw_ptr<const apps::mojom::App> mojom_state_ = nullptr;
  raw_ptr<const apps::mojom::App> mojom_delta_ = nullptr;

  raw_ptr<const apps::App> state_ = nullptr;
  raw_ptr<const apps::App> delta_ = nullptr;

  const ::AccountId& account_id_;
};

// For logging and debug purposes.
COMPONENT_EXPORT(APP_UPDATE)
std::ostream& operator<<(std::ostream& out, const AppUpdate& app);

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_UPDATE_H_
