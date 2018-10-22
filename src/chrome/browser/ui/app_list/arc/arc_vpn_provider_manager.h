// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_ARC_ARC_VPN_PROVIDER_MANAGER_H_
#define CHROME_BROWSER_UI_APP_LIST_ARC_ARC_VPN_PROVIDER_MANAGER_H_

// Helper class to create VPN provider specific events to observers.

#include <memory>
#include <string>

#include "base/observer_list.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "components/keyed_service/core/keyed_service.h"

namespace app_list {

class ArcVpnProviderManager : public ArcAppListPrefs::Observer,
                              public KeyedService {
 public:
  struct ArcVpnProvider {
    ArcVpnProvider(const std::string& app_name,
                   const std::string& package_name,
                   const std::string& app_id,
                   const base::Time last_launch_time);
    ~ArcVpnProvider();

    const std::string app_name;
    const std::string package_name;
    const std::string app_id;
    const base::Time last_launch_time;
  };

  class Observer {
   public:
    // Notifies initial refresh of Arc VPN providers.
    virtual void OnArcVpnProvidersRefreshed(
        const std::vector<std::unique_ptr<ArcVpnProvider>>& arc_vpn_providers) {
    }
    // Notifies removal of an Arc VPN provider.
    virtual void OnArcVpnProviderRemoved(const std::string& package_name) {}
    // Notifies update for an Arc VPN provider. Update includes newly
    // installation, name update, launch time update.
    virtual void OnArcVpnProviderUpdated(ArcVpnProvider* arc_vpn_provider) {}

   protected:
    virtual ~Observer() {}
  };

  static ArcVpnProviderManager* Get(content::BrowserContext* context);

  static ArcVpnProviderManager* Create(content::BrowserContext* context);

  ~ArcVpnProviderManager() override;

  // ArcAppListPrefs Observer:
  void OnAppNameUpdated(const std::string& id,
                        const std::string& name) override;
  void OnAppLastLaunchTimeUpdated(const std::string& app_id) override;
  void OnPackageInstalled(
      const arc::mojom::ArcPackageInfo& package_info) override;
  void OnPackageRemoved(const std::string& package_name,
                        bool uninstalled) override;
  void OnPackageListInitialRefreshed() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  std::vector<std::unique_ptr<ArcVpnProvider>> GetArcVpnProviders();

 private:
  explicit ArcVpnProviderManager(ArcAppListPrefs* arc_app_list_prefs);

  void MaybeNotifyArcVpnProviderUpdate(const std::string& app_id);

  ArcAppListPrefs* const arc_app_list_prefs_;

  // List of observers.
  base::ObserverList<Observer>::Unchecked observer_list_;

  DISALLOW_COPY_AND_ASSIGN(ArcVpnProviderManager);
};

}  // namespace app_list

#endif  //  CHROME_BROWSER_UI_APP_LIST_ARC_ARC_VPN_PROVIDER_MANAGER_H_
