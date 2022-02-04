// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEEDBACK_PII_TYPES_H_
#define COMPONENTS_FEEDBACK_PII_TYPES_H_

namespace feedback {

// PII (Personally Identifiable Information) types that can be detected in the
// debug data.
enum class PIIType {
  // Indicates no PII. Mainly for testing.
  kNone,
  // Android App Storage paths. The path starts with either
  // /home/root/<hash>/data/data/<package_name>/ or
  // /home/root/<hash>/data/user_de/<number>/<package_name>/, the path
  // components following <package_name>/ are app specific and will be
  // considered as PII sensitive data.
  kAndroidAppStoragePath,
  // BSSID (Basic Service Set Identifier) of a wifi service.
  kBSSID,
  // Unique identifier of the cell of the Cell tower object that's used by
  // ModemManager.
  kCellID,
  // Email addresses.
  kEmail,
  // GAIA (Google Accounts and ID Administration) ID. Gaia ID is a 64-bit
  // integer.
  kGaiaID,
  // Hexadecimal strings of length 32, 40 and 64 are considered to be hashes.
  kHash,
  // IPP (Internet Printing Protocol) Addresses.
  kIPPAddress,
  // IP (Internet Protocol) address. Stores data in two versions: IPv4 (e.g.
  // 127.0.0.1) or IPv6 (e.g. 2001:0db8:85a3:0000:0000:8a2e:0370:7334).
  kIPAddress,
  // The Location Area Code (LAC) for GSM and WCDMA networks of the Cell tower
  // object that's used by ModemManager.
  kLocationAreaCode,
  // MAC address is a unique identifier assigned to a network interface
  // controller (NIC) for use as a network address in communications within a
  // network segment (e.g 00:00:5e:00:53:af). MAC addresses with general meaning
  // like ARP failure result MAC and Broadcast MAC won't be treated as PII
  // sensitive data and won't be included in this category.
  kMACAddress,
  // Window titles that appear in UI hierarchy.
  kUIHierarchyWindowTitles,
  // URLs that can appear in logs.
  kURL,
  // Universal Unique Identifiers (UUIDs). UUID can also be given by 'blkid',
  // 'lvs' and 'pvs' tools.
  kUUID,
  // Serial numbers.
  kSerial,
  // SSID (Service Set Identifier) of wifi networks can be detected in the logs
  // provided by wpa_supplicant and shill.
  kSSID,
  // Volume labels presented in the 'blkid' tool, and as part of removable
  // media paths shown in various logs such as cros-disks (in syslog).
  kVolumeLabel,
};

}  // namespace feedback

#endif  // COMPONENTS_FEEDBACK_PII_TYPES_H_