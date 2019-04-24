// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_UTIL_H_

#include <string>

class Profile;

namespace plugin_vm {

// Checks if PluginVm is allowed for the current profile.
bool IsPluginVmAllowedForProfile(Profile* profile);
// Checks if PluginVm is configured for the current profile.
bool IsPluginVmConfigured(Profile* profile);

void ShowPluginVmLauncherView(Profile* profile);

// Retrieves the license key to be used for PluginVm. If
// none is set this will return an empty string.
std::string GetPluginVmLicenseKey();

}  // namespace plugin_vm

#endif  // CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_UTIL_H_
