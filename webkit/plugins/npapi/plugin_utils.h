// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_NPAPI_PLUGIN_UTILS_H_
#define WEBKIT_PLUGINS_NPAPI_PLUGIN_UTILS_H_

#include "base/strings/string16.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
class Version;
}

namespace webkit {
namespace npapi {

// Parse a version string as used by a plug-in. This method is more lenient
// in accepting weird version strings than base::Version::GetFromString()
void CreateVersionFromString(
    const base::string16& version_string,
    base::Version* parsed_version);

// Returns true iff NPAPI plugins are supported on the current platform.
bool NPAPIPluginsSupported();

// Tells the plugin thread to terminate the process forcefully instead of
// exiting cleanly.
void SetForcefullyTerminatePluginProcess(bool value);

// Returns true if the plugin thread should terminate the process forcefully
// instead of exiting cleanly.
bool ShouldForcefullyTerminatePluginProcess();

#if defined(OS_WIN)
// The window class name for a plugin window.
extern const char16 kNativeWindowClassName[];
extern const char16 kPluginNameAtomProperty[];
extern const char16 kPluginVersionAtomProperty[];
extern const char16 kDummyActivationWindowName[];

bool IsPluginDelegateWindow(HWND window);
bool GetPluginNameFromWindow(HWND window, base::string16* plugin_name);
bool GetPluginVersionFromWindow(HWND window, base::string16* plugin_version);

// Returns true if the window handle passed in is that of the dummy
// activation window for windowless plugins.
bool IsDummyActivationWindow(HWND window);

// Returns the default HWND to parent the windowed plugins and dummy windows
// for activation to when none isavailable.
HWND GetDefaultWindowParent();
#endif

}  // namespace npapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_NPAPI_PLUGIN_UTILS_H_
