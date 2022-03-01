// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/chromebox_for_meetings/network_settings/network_settings_service.h"

#include <utility>

#include "chrome/browser/ui/webui/chromeos/chromebox_for_meetings/network_settings_dialog.h"
#include "chromeos/dbus/chromebox_for_meetings/cfm_hotline_client.h"

namespace chromeos {
namespace cfm {

namespace {
NetworkSettingsService* g_network_settings_service = nullptr;
}  // namespace

NetworkSettingsService::NetworkSettingsService()
    : service_adaptor_(mojom::CfmNetworkSettings::Name_, this) {
  CfmHotlineClient::Get()->AddObserver(this);
}

NetworkSettingsService::~NetworkSettingsService() {
  CfmHotlineClient::Get()->RemoveObserver(this);
}

void NetworkSettingsService::Initialize() {
  CHECK(!g_network_settings_service);
  g_network_settings_service = new NetworkSettingsService();
}

void NetworkSettingsService::Shutdown() {
  CHECK(g_network_settings_service);
  delete g_network_settings_service;
  g_network_settings_service = nullptr;
}

NetworkSettingsService* NetworkSettingsService::Get() {
  return g_network_settings_service;
}

bool NetworkSettingsService::IsInitialized() {
  return g_network_settings_service;
}

void NetworkSettingsService::ShowDialog() {
  if (!NetworkSettingsDialog::IsShown()) {
    NetworkSettingsDialog::ShowDialog();
  }
}

bool NetworkSettingsService::ServiceRequestReceived(
    const std::string& interface_name) {
  if (interface_name != mojom::CfmNetworkSettings::Name_) {
    return false;
  }
  service_adaptor_.BindServiceAdaptor();
  return true;
}

void NetworkSettingsService::OnBindService(
    mojo::ScopedMessagePipeHandle receiver_pipe) {
  receivers_.Add(this, mojo::PendingReceiver<mojom::CfmNetworkSettings>(
                           std::move(receiver_pipe)));
}

void NetworkSettingsService::OnAdaptorDisconnect() {
  receivers_.Clear();
}

}  // namespace cfm
}  // namespace chromeos
