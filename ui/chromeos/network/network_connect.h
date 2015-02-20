// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_CHROMEOS_NETWORK_NETWORK_CONNECT_H
#define UI_CHROMEOS_NETWORK_NETWORK_CONNECT_H

#include <string>

#include "base/strings/string16.h"
#include "ui/chromeos/ui_chromeos_export.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {
class NetworkTypePattern;
}

namespace ui {

class UI_CHROMEOS_EXPORT NetworkConnect {
 public:
  class Delegate {
   public:
    // Shows UI to configure or activate the network specified by |network_id|,
    // which may include showing Payment or Portal UI when appropriate.
    virtual void ShowNetworkConfigure(const std::string& network_id) = 0;

    // Shows the settings related to network. If |network_id| is not empty,
    // show the settings for that network.
    virtual void ShowNetworkSettings(const std::string& network_id) = 0;

    // Shows UI to enroll the network specified by |network_id| if appropriate
    // and returns true, otherwise returns false.
    virtual bool ShowEnrollNetwork(const std::string& network_id) = 0;

    // Shows UI to unlock a mobile sim.
    virtual void ShowMobileSimDialog() = 0;

    // Shows UI to setup a mobile network.
    virtual void ShowMobileSetupDialog(const std::string& service_path) = 0;

   protected:
    virtual ~Delegate() {}
  };

  // Creates the global NetworkConnect object. |delegate| is owned by the
  // caller.
  static void Initialize(Delegate* delegate);

  // Destroys the global NetworkConnect object.
  static void Shutdown();

  // Returns the global NetworkConnect object if initialized or NULL.
  static NetworkConnect* Get();

  static const char kErrorActivateFailed[];

  virtual ~NetworkConnect();

  // Requests a network connection and handles any errors and notifications.
  virtual void ConnectToNetwork(const std::string& service_path) = 0;

  // Enables or disables a network technology. If |technology| refers to
  // cellular and the device cannot be enabled due to a SIM lock, this function
  // will launch the SIM unlock dialog.
  virtual void SetTechnologyEnabled(
      const chromeos::NetworkTypePattern& technology,
      bool enabled_state) = 0;

  // Requests network activation and handles any errors and notifications.
  virtual void ActivateCellular(const std::string& service_path) = 0;

  // Determines whether or not a network requires a connection to activate or
  // setup and either shows a notification or opens the mobile setup dialog.
  virtual void ShowMobileSetup(const std::string& service_path) = 0;

  // Configures a network with a dictionary of Shill properties, then sends a
  // connect request. The profile is set according to 'shared' if allowed.
  virtual void ConfigureNetworkAndConnect(
      const std::string& service_path,
      const base::DictionaryValue& shill_properties,
      bool shared) = 0;

  // Requests a new network configuration to be created from a dictionary of
  // Shill properties and sends a connect request if the configuration succeeds.
  // The profile used is determined by |shared|.
  virtual void CreateConfigurationAndConnect(
      base::DictionaryValue* shill_properties,
      bool shared) = 0;

  // Requests a new network configuration to be created from a dictionary of
  // Shill properties. The profile used is determined by |shared|.
  virtual void CreateConfiguration(base::DictionaryValue* shill_properties,
                                   bool shared) = 0;

  // Returns the localized string for shill error string |error|.
  virtual base::string16 GetShillErrorString(
      const std::string& error,
      const std::string& service_path) = 0;

  // Shows the settings for the network specified by |service_path|. If empty,
  // or no matching network exists, shows the general internet settings page.
  virtual void ShowNetworkSettings(const std::string& service_path) = 0;

 protected:
  NetworkConnect();

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkConnect);
};

}  // ui

#endif  // UI_CHROMEOS_NETWORK_NETWORK_CONNECT_H
