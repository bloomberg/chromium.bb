// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSTEM_API_DBUS_PATCHPANEL_DBUS_CONSTANTS_H_
#define SYSTEM_API_DBUS_PATCHPANEL_DBUS_CONSTANTS_H_

namespace patchpanel {

const char kPatchPanelInterface[] = "org.chromium.PatchPanel";
const char kPatchPanelServicePath[] = "/org/chromium/PatchPanel";
const char kPatchPanelServiceName[] = "org.chromium.PatchPanel";

// Exported methods.
const char kArcStartupMethod[] = "ArcStartup";
const char kArcShutdownMethod[] = "ArcShutdown";
const char kArcVmStartupMethod[] = "ArcVmStartup";
const char kArcVmShutdownMethod[] = "ArcVmShutdown";
const char kTerminaVmStartupMethod[] = "TerminaVmStartup";
const char kTerminaVmShutdownMethod[] = "TerminaVmShutdown";
const char kPluginVmStartupMethod[] = "PluginVmStartup";
const char kPluginVmShutdownMethod[] = "PluginVmShutdown";
const char kSetVpnIntentMethod[] = "SetVpnIntent";
const char kConnectNamespaceMethod[] = "ConnectNamespace";

}  // namespace patchpanel

#endif  // SYSTEM_API_DBUS_PATCHPANEL_DBUS_CONSTANTS_H_
