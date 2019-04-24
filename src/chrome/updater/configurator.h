// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_CONFIGURATOR_H_
#define CHROME_UPDATER_CONFIGURATOR_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/update_client/configurator.h"

class GURL;
class PrefService;

namespace base {
class Version;
}  // namespace base

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace update_client {
class ActivityDataService;
class NetworkFetcherFactory;
class ProtocolHandlerFactory;
}  // namespace update_client

namespace updater {

class Configurator : public update_client::Configurator {
 public:
  explicit Configurator(
      std::unique_ptr<service_manager::Connector> connector_prototype);

  // Configurator for update_client::Configurator.
  int InitialDelay() const override;
  int NextCheckDelay() const override;
  int OnDemandDelay() const override;
  int UpdateDelay() const override;
  std::vector<GURL> UpdateUrl() const override;
  std::vector<GURL> PingUrl() const override;
  std::string GetProdId() const override;
  base::Version GetBrowserVersion() const override;
  std::string GetChannel() const override;
  std::string GetBrand() const override;
  std::string GetLang() const override;
  std::string GetOSLongName() const override;
  base::flat_map<std::string, std::string> ExtraRequestParams() const override;
  std::string GetDownloadPreference() const override;
  scoped_refptr<update_client::NetworkFetcherFactory> GetNetworkFetcherFactory()
      override;
  std::unique_ptr<service_manager::Connector> CreateServiceManagerConnector()
      const override;
  bool EnabledDeltas() const override;
  bool EnabledComponentUpdates() const override;
  bool EnabledBackgroundDownloader() const override;
  bool EnabledCupSigning() const override;
  PrefService* GetPrefService() const override;
  update_client::ActivityDataService* GetActivityDataService() const override;
  bool IsPerUserInstall() const override;
  std::vector<uint8_t> GetRunActionKeyHash() const override;
  std::string GetAppGuid() const override;
  std::unique_ptr<update_client::ProtocolHandlerFactory>
  GetProtocolHandlerFactory() const override;
  update_client::RecoveryCRXElevator GetRecoveryCRXElevator() const override;

 private:
  friend class base::RefCountedThreadSafe<Configurator>;
  ~Configurator() override;

  // connector_prototype__ is a connector to the updater's service_manager,
  // through which mojo calls can be made. Connectors have thread-affinity, so
  // this connector serves as a template that can be cloned for specific uses.
  std::unique_ptr<service_manager::Connector> connector_prototype_;

  scoped_refptr<update_client::NetworkFetcherFactory> network_fetcher_factory_;
  DISALLOW_COPY_AND_ASSIGN(Configurator);
};

}  // namespace updater

#endif  // CHROME_UPDATER_CONFIGURATOR_H_
