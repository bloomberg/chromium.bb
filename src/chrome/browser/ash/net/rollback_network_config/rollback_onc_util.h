// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_ROLLBACK_NETWORK_CONFIG_ROLLBACK_ONC_UTIL_H_
#define CHROME_BROWSER_ASH_NET_ROLLBACK_NETWORK_CONFIG_ROLLBACK_ONC_UTIL_H_

#include "base/values.h"

// Rollback-specific functions for managed and non-managed onc configurations
// of wifi and ethernet. These functions do not validate the onc format. To be
// used only with validated input, e.g. configurations coming from Chrome.
// Look for onc_spec.md for documentation of the onc format.
namespace ash {
namespace rollback_network_config {

// Returns the value of the given key. This will crash if string value can not
// be found.
std::string GetStringValue(const base::Value& network, const std::string& key);

// Managed ONC

// Collapses a managed onc dictionary to the values that are marked as active
// configuration. Values that are not split into managed dictionaries are kept.
void ManagedOncCollapseToActive(base::Value* network);

// Collapses a managed onc dictionary to the values that are marked as shared
// setting. Values that are not split into managed dictionaries are discarded.
// This results in the part that shill saves as UI data for the given managed
// network.
void ManagedOncCollapseToUiData(base::Value* network);

// Sets the PSK password of a managed onc dictionary as device or policy
// configured, depending on the source of the network.
void ManagedOncWiFiSetPskPassword(base::Value* network,
                                  const std::string& password);
// Sets the EAP password of a managed onc dictionary as device or policy
// configured, depending on the source of the network.
void ManagedOncSetEapPassword(base::Value* network,
                              const std::string& password);

// ONC

bool OncIsWiFi(const base::Value& network);
bool OncIsEthernet(const base::Value& network);

bool OncIsSourceDevicePolicy(const base::Value& network);
bool OncIsSourceDevice(const base::Value& network);

bool OncHasNoSecurity(const base::Value& network);

// Returns true if the given network is of a type that would require an EAP
// entry.
bool OncIsEap(const base::Value& network);

// Returns true if the given dictionary has an EAP entry.
bool OncHasEapConfiguration(const base::Value& network);

// Returns true if the given network is an EAP network and is of a type that
// does not require a client certificate.
bool OncIsEapWithoutClientCertificate(const base::Value& network);

// ONC>EAP
// The following functions only succeed if called on a dictionary containing
// an EAP entry within a wifi or ethernet entry.

std::string OncGetEapIdentity(const base::Value& network);
std::string OncGetEapInner(const base::Value& network);
std::string OncGetEapOuter(const base::Value& network);
std::string OncGetEapPassword(const base::Value& network);
std::string OncGetEapClientCertType(const base::Value& network);
std::string OncGetEapClientCertPKCS11Id(const base::Value& network);

// Returns true if the given EAP wifi or ethernet is of a type that may require
// a client certificate.
bool OncEapRequiresClientCertificate(const base::Value& network);

void OncSetEapPassword(base::Value* network, const std::string& password);

// ONC>WiFi
// The following functions only succeed if called on a dictionary containing
// a wifi entry.

std::string OncWiFiGetSecurity(const base::Value& network);
std::string OncWiFiGetPassword(const base::Value& network);

bool OncWiFiIsPsk(const base::Value& network);

void OncWiFiSetPskPassword(base::Value* network, const std::string& password);

// ONC>Ethernet
// The following functions only succeed if called on a dictionary containing
// an ethernet entry.

std::string OncEthernetGetAuthentication(const base::Value& network);

}  // namespace rollback_network_config
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_ROLLBACK_NETWORK_CONFIG_ROLLBACK_ONC_UTIL_H_
