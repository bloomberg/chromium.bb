// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PREFERRED_APPS_LIST_HANDLE_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PREFERRED_APPS_LIST_HANDLE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace apps {

// A read-only handle to the preferred apps list set by the user. This class
// allows for observation of the list and accessor methods. To make changes to
// the list, use App Service Proxy methods.
class PreferredAppsListHandle {
 public:
  PreferredAppsListHandle();
  ~PreferredAppsListHandle();

  PreferredAppsListHandle(const PreferredAppsListHandle&) = delete;
  PreferredAppsListHandle& operator=(const PreferredAppsListHandle&) = delete;

  using PreferredApps = std::vector<apps::mojom::PreferredAppPtr>;

  virtual bool IsInitialized() const = 0;
  // Get the entry size of the preferred app list.
  virtual size_t GetEntrySize() const = 0;
  // Get a copy of the preferred apps.
  virtual PreferredApps GetValue() const = 0;
  virtual const PreferredApps& GetReference() const = 0;

  virtual bool IsPreferredAppForSupportedLinks(
      const std::string& app_id) const = 0;

  // Find preferred app id for an |url|.
  virtual absl::optional<std::string> FindPreferredAppForUrl(
      const GURL& url) const = 0;

  // Find preferred app id for an |intent|.
  virtual absl::optional<std::string> FindPreferredAppForIntent(
      const apps::mojom::IntentPtr& intent) const = 0;

  // Returns a list of app IDs that are set as preferred app to an intent
  // filter in the |intent_filters| list.
  virtual base::flat_set<std::string> FindPreferredAppsForFilters(
      const std::vector<apps::mojom::IntentFilterPtr>& intent_filters)
      const = 0;

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnPreferredAppChanged(const std::string& app_id,
                                       bool is_preferred_app) = 0;

    // Called when the PreferredAppsList object (the thing that this observer
    // observes) will be destroyed. In response, the observer, |this|, should
    // call "cache->RemoveObserver(this)", whether directly or indirectly (e.g.
    // via base::ScopedObservation::Remove or via Observe(nullptr)).
    virtual void OnPreferredAppsListWillBeDestroyed(
        PreferredAppsListHandle* handle) = 0;

    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

   protected:
    // Use this constructor when the observer |this| is tied to a single
    // PreferredAppsList for its entire lifetime, or until the observee (the
    // PreferredAppsList) is destroyed, whichever comes first.
    explicit Observer(PreferredAppsListHandle* handle);

    // Use this constructor when the observer |this| wants to observe a
    // PreferredAppsList for part of its lifetime. It can then call Observe() to
    // start and stop observing.
    Observer();
    ~Observer() override;

    // Start observing a different PreferredAppsList. |cache| may be nullptr,
    // meaning to stop observing.
    void Observe(PreferredAppsListHandle* handle);

   private:
    raw_ptr<PreferredAppsListHandle> handle_ = nullptr;
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  base::ObserverList<Observer> observers_;
};

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_PREFERRED_APPS_LIST_HANDLE_H_
