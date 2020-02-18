// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/component_updater/ios_component_updater_configurator.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/version.h"
#include "components/component_updater/component_updater_command_line_config_policy.h"
#include "components/component_updater/configurator_impl.h"
#include "components/services/patch/in_process_file_patcher.h"
#include "components/services/unzip/in_process_unzipper.h"
#include "components/update_client/activity_data_service.h"
#include "components/update_client/net/network_chromium.h"
#include "components/update_client/patch/patch_impl.h"
#include "components/update_client/patcher.h"
#include "components/update_client/protocol_handler.h"
#include "components/update_client/unzip/unzip_impl.h"
#include "components/update_client/unzipper.h"
#include "components/update_client/update_query_params.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/google/google_brand.h"
#include "ios/chrome/common/channel_info.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace component_updater {

namespace {

class IOSConfigurator : public update_client::Configurator {
 public:
  explicit IOSConfigurator(const base::CommandLine* cmdline);

  // update_client::Configurator overrides.
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
  scoped_refptr<update_client::UnzipperFactory> GetUnzipperFactory() override;
  scoped_refptr<update_client::PatcherFactory> GetPatcherFactory() override;
  bool EnabledDeltas() const override;
  bool EnabledComponentUpdates() const override;
  bool EnabledBackgroundDownloader() const override;
  bool EnabledCupSigning() const override;
  PrefService* GetPrefService() const override;
  update_client::ActivityDataService* GetActivityDataService() const override;
  bool IsPerUserInstall() const override;
  std::unique_ptr<update_client::ProtocolHandlerFactory>
  GetProtocolHandlerFactory() const override;

 private:
  friend class base::RefCountedThreadSafe<IOSConfigurator>;

  ConfiguratorImpl configurator_impl_;
  scoped_refptr<update_client::NetworkFetcherFactory> network_fetcher_factory_;
  scoped_refptr<update_client::UnzipperFactory> unzip_factory_;
  scoped_refptr<update_client::PatcherFactory> patch_factory_;

  ~IOSConfigurator() override {}
};

// Allows the component updater to use non-encrypted communication with the
// update backend. The security of the update checks is enforced using
// a custom message signing protocol and it does not depend on using HTTPS.
IOSConfigurator::IOSConfigurator(const base::CommandLine* cmdline)
    : configurator_impl_(ComponentUpdaterCommandLineConfigPolicy(cmdline),
                         false) {}

int IOSConfigurator::InitialDelay() const {
  return configurator_impl_.InitialDelay();
}

int IOSConfigurator::NextCheckDelay() const {
  return configurator_impl_.NextCheckDelay();
}

int IOSConfigurator::OnDemandDelay() const {
  return configurator_impl_.OnDemandDelay();
}

int IOSConfigurator::UpdateDelay() const {
  return configurator_impl_.UpdateDelay();
}

std::vector<GURL> IOSConfigurator::UpdateUrl() const {
  return configurator_impl_.UpdateUrl();
}

std::vector<GURL> IOSConfigurator::PingUrl() const {
  return configurator_impl_.PingUrl();
}

std::string IOSConfigurator::GetProdId() const {
  return update_client::UpdateQueryParams::GetProdIdString(
      update_client::UpdateQueryParams::ProdId::CHROME);
}

base::Version IOSConfigurator::GetBrowserVersion() const {
  return configurator_impl_.GetBrowserVersion();
}

std::string IOSConfigurator::GetChannel() const {
  return GetChannelString();
}

std::string IOSConfigurator::GetBrand() const {
  std::string brand;
  ios::google_brand::GetBrand(&brand);
  return brand;
}

std::string IOSConfigurator::GetLang() const {
  return GetApplicationContext()->GetApplicationLocale();
}

std::string IOSConfigurator::GetOSLongName() const {
  return configurator_impl_.GetOSLongName();
}

base::flat_map<std::string, std::string> IOSConfigurator::ExtraRequestParams()
    const {
  return configurator_impl_.ExtraRequestParams();
}

std::string IOSConfigurator::GetDownloadPreference() const {
  return configurator_impl_.GetDownloadPreference();
}

scoped_refptr<update_client::NetworkFetcherFactory>
IOSConfigurator::GetNetworkFetcherFactory() {
  if (!network_fetcher_factory_) {
    network_fetcher_factory_ =
        base::MakeRefCounted<update_client::NetworkFetcherChromiumFactory>(
            GetApplicationContext()->GetSharedURLLoaderFactory(),
            // Never send cookies for component update downloads.
            base::BindRepeating([](const GURL& url) { return false; }));
  }
  return network_fetcher_factory_;
}

scoped_refptr<update_client::UnzipperFactory>
IOSConfigurator::GetUnzipperFactory() {
  if (!unzip_factory_) {
    unzip_factory_ = base::MakeRefCounted<update_client::UnzipChromiumFactory>(
        base::BindRepeating(&unzip::LaunchInProcessUnzipper));
  }
  return unzip_factory_;
}

scoped_refptr<update_client::PatcherFactory>
IOSConfigurator::GetPatcherFactory() {
  if (!patch_factory_) {
    patch_factory_ = base::MakeRefCounted<update_client::PatchChromiumFactory>(
        base::BindRepeating(&patch::LaunchInProcessFilePatcher));
  }
  return patch_factory_;
}

bool IOSConfigurator::EnabledDeltas() const {
  return configurator_impl_.EnabledDeltas();
}

bool IOSConfigurator::EnabledComponentUpdates() const {
  return configurator_impl_.EnabledComponentUpdates();
}

bool IOSConfigurator::EnabledBackgroundDownloader() const {
  return configurator_impl_.EnabledBackgroundDownloader();
}

bool IOSConfigurator::EnabledCupSigning() const {
  return configurator_impl_.EnabledCupSigning();
}

PrefService* IOSConfigurator::GetPrefService() const {
  return GetApplicationContext()->GetLocalState();
}

update_client::ActivityDataService* IOSConfigurator::GetActivityDataService()
    const {
  return nullptr;
}

bool IOSConfigurator::IsPerUserInstall() const {
  return true;
}

std::unique_ptr<update_client::ProtocolHandlerFactory>
IOSConfigurator::GetProtocolHandlerFactory() const {
  return configurator_impl_.GetProtocolHandlerFactory();
}

}  // namespace

scoped_refptr<update_client::Configurator> MakeIOSComponentUpdaterConfigurator(
    const base::CommandLine* cmdline) {
  return base::MakeRefCounted<IOSConfigurator>(cmdline);
}

}  // namespace component_updater
