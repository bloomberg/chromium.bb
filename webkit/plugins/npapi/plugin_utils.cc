// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/npapi/plugin_utils.h"

#include <algorithm>

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/version.h"

#if defined(OS_WIN)
#include "ui/base/win/hwnd_util.h"
#endif

namespace webkit {
namespace npapi {

// Global variable used by the plugin quirk "die after unload".
bool g_forcefully_terminate_plugin_process = false;

void CreateVersionFromString(const base::string16& version_string,
                             Version* parsed_version) {
  // Remove spaces and ')' from the version string,
  // Replace any instances of 'r', ',' or '(' with a dot.
  std::string version = UTF16ToASCII(version_string);
  RemoveChars(version, ") ", &version);
  std::replace(version.begin(), version.end(), 'd', '.');
  std::replace(version.begin(), version.end(), 'r', '.');
  std::replace(version.begin(), version.end(), ',', '.');
  std::replace(version.begin(), version.end(), '(', '.');
  std::replace(version.begin(), version.end(), '_', '.');

  // Remove leading zeros from each of the version components.
  std::string no_leading_zeros_version;
  std::vector<std::string> numbers;
  base::SplitString(version, '.', &numbers);
  for (size_t i = 0; i < numbers.size(); ++i) {
    size_t n = numbers[i].size();
    size_t j = 0;
    while (j < n && numbers[i][j] == '0') {
      ++j;
    }
    no_leading_zeros_version += (j < n) ? numbers[i].substr(j) : "0";
    if (i != numbers.size() - 1) {
      no_leading_zeros_version += ".";
    }
  }

  *parsed_version = Version(no_leading_zeros_version);
}

bool NPAPIPluginsSupported() {
#if defined(OS_WIN) || defined(OS_MACOSX) || (defined(OS_LINUX) && !defined(USE_AURA))
  return true;
#else
  return false;
#endif
}

void SetForcefullyTerminatePluginProcess(bool value) {
  g_forcefully_terminate_plugin_process = value;
}

bool ShouldForcefullyTerminatePluginProcess() {
  return g_forcefully_terminate_plugin_process;
}

#if defined(OS_WIN)

const char16 kNativeWindowClassName[] = L"NativeWindowClass";
const char16 kPluginNameAtomProperty[] = L"PluginNameAtom";
const char16 kPluginVersionAtomProperty[] = L"PluginVersionAtom";
const char16 kDummyActivationWindowName[] = L"DummyWindowForActivation";

namespace {

bool GetPluginPropertyFromWindow(
    HWND window, const wchar_t* plugin_atom_property,
    base::string16* plugin_property) {
  ATOM plugin_atom = reinterpret_cast<ATOM>(
      GetPropW(window, plugin_atom_property));
  if (plugin_atom != 0) {
    WCHAR plugin_property_local[MAX_PATH] = {0};
    GlobalGetAtomNameW(plugin_atom,
                       plugin_property_local,
                       ARRAYSIZE(plugin_property_local));
    *plugin_property = plugin_property_local;
    return true;
  }
  return false;
}

}  // namespace

bool IsPluginDelegateWindow(HWND window) {
  return ui::GetClassName(window) == base::string16(kNativeWindowClassName);
}

// static
bool GetPluginNameFromWindow(
    HWND window, base::string16* plugin_name) {
  return IsPluginDelegateWindow(window) &&
      GetPluginPropertyFromWindow(
          window, kPluginNameAtomProperty, plugin_name);
}

// static
bool GetPluginVersionFromWindow(
    HWND window, base::string16* plugin_version) {
  return IsPluginDelegateWindow(window) &&
      GetPluginPropertyFromWindow(
          window, kPluginVersionAtomProperty, plugin_version);
}

bool IsDummyActivationWindow(HWND window) {
  if (!IsWindow(window))
    return false;

  wchar_t window_title[MAX_PATH + 1] = {0};
  if (GetWindowText(window, window_title, arraysize(window_title))) {
    return (0 == lstrcmpiW(window_title, kDummyActivationWindowName));
  }
  return false;
}

HWND GetDefaultWindowParent() {
  return GetDesktopWindow();
}

#endif

}  // namespace npapi
}  // namespace webkit
