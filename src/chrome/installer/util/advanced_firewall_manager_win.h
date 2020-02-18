// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_ADVANCED_FIREWALL_MANAGER_WIN_H_
#define CHROME_INSTALLER_UTIL_ADVANCED_FIREWALL_MANAGER_WIN_H_

#include <windows.h>
#include <netfw.h>
#include <stdint.h>
#include <wrl/client.h>

#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/strings/string16.h"

namespace installer {

// Manages firewall rules using Advanced Security Windows API. The API is
// available on Windows Vista and later. Most methods need elevation.
class AdvancedFirewallManager {
 public:
  AdvancedFirewallManager();
  ~AdvancedFirewallManager();

  // Initializes object to manage application win name |app_name| and path
  // |app_path|.
  bool Init(const base::string16& app_name, const base::FilePath& app_path);

  // Returns true if firewall is enabled.
  bool IsFirewallEnabled();

  // Returns true if there is any rule for the application.
  bool HasAnyRule();

  // Adds a firewall rule allowing inbound connections to the application on UDP
  // port |port|. Replaces the rule if it already exists. Needs elevation.
  bool AddUDPRule(const base::string16& rule_name,
                  const base::string16& description,
                  uint16_t port);

  // Deletes all rules with specified name. Needs elevation.
  void DeleteRuleByName(const base::string16& rule_name);

  // Deletes all rules for current app. Needs elevation.
  void DeleteAllRules();

 private:
  friend class AdvancedFirewallManagerTest;

  // Creates a firewall rule allowing inbound connections to UDP port |port|.
  Microsoft::WRL::ComPtr<INetFwRule> CreateUDPRule(
      const base::string16& rule_name,
      const base::string16& description,
      uint16_t port);

  // Returns the list of rules applying to the application.
  void GetAllRules(std::vector<Microsoft::WRL::ComPtr<INetFwRule>>* rules);

  // Deletes rules. Needs elevation.
  void DeleteRule(Microsoft::WRL::ComPtr<INetFwRule> rule);

  base::string16 app_name_;
  base::FilePath app_path_;
  Microsoft::WRL::ComPtr<INetFwPolicy2> firewall_policy_;
  Microsoft::WRL::ComPtr<INetFwRules> firewall_rules_;

  DISALLOW_COPY_AND_ASSIGN(AdvancedFirewallManager);
};

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_ADVANCED_FIREWALL_MANAGER_WIN_H_
