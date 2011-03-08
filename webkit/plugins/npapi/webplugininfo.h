// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_NPAPI_WEBPLUGININFO_H_
#define WEBKIT_PLUGINS_NPAPI_WEBPLUGININFO_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"

namespace webkit {
namespace npapi {

// Describes a mime type entry for a plugin.
// TODO(viettrungluu): This isn't NPAPI-specific. Move this somewhere else.
struct WebPluginMimeType {
  WebPluginMimeType();
  // A constructor for the common case of a single file extension and an ASCII
  // description.
  WebPluginMimeType(const std::string& m,
                    const std::string& f,
                    const std::string& d);
  ~WebPluginMimeType();

  // The name of the mime type (e.g., "application/x-shockwave-flash").
  std::string mime_type;

  // A list of all the file extensions for this mime type.
  std::vector<std::string> file_extensions;

  // Description of the mime type.
  string16 description;
};

// Describes an available NPAPI plugin.
struct WebPluginInfo {
  // Defines the possible enabled state a plugin can have.
  // The enum values actually represent a 3-bit bitfield :
  // |PE|PD|U| - where |PE|PD| is policy state and U is user state.
  //    PE == 1 means the plugin is forced to enabled state by policy
  //    PD == 1 means the plugin is forced to disabled by policy
  // PE and PD CAN'T be both 1 but can be both 0 which mean no policy is set.
  //     U == 1 means the user has disabled the plugin.
  // Because the plugin user state might have been changed before a policy was
  // introduced the user state might contradict the policy state in which case
  // the policy has precedence.
  enum EnabledStates {
    USER_ENABLED = 0,
    USER_DISABLED = 1 << 0,
    POLICY_DISABLED = 1 << 1,
    POLICY_ENABLED = 1 << 2,
    USER_ENABLED_POLICY_UNMANAGED = USER_ENABLED,
    USER_ENABLED_POLICY_DISABLED = USER_ENABLED| POLICY_DISABLED,
    USER_ENABLED_POLICY_ENABLED = USER_ENABLED | POLICY_ENABLED,
    USER_DISABLED_POLICY_UNMANAGED = USER_DISABLED,
    USER_DISABLED_POLICY_DISABLED = USER_DISABLED | POLICY_DISABLED,
    USER_DISABLED_POLICY_ENABLED = USER_DISABLED | POLICY_ENABLED,
    USER_MASK = USER_DISABLED,
    MANAGED_MASK = POLICY_DISABLED | POLICY_ENABLED,
    POLICY_UNMANAGED = -1
  };

  WebPluginInfo();
  WebPluginInfo(const WebPluginInfo& rhs);
  ~WebPluginInfo();
  WebPluginInfo& operator=(const WebPluginInfo& rhs);

  // Special constructor only used during unit testing:
  WebPluginInfo(const string16& fake_name,
                const FilePath& fake_path,
                const string16& fake_version,
                const string16& fake_desc);

  // The name of the plugin (i.e. Flash).
  string16 name;

  // The path to the plugin file (DLL/bundle/library).
  FilePath path;

  // The version number of the plugin file (may be OS-specific)
  string16 version;

  // A description of the plugin that we get from its version info.
  string16 desc;

  // A list of all the mime types that this plugin supports.
  std::vector<WebPluginMimeType> mime_types;

  // Enabled state of the plugin. See the EnabledStates enum.
  int enabled;
};

// Checks whether a plugin is enabled either by the user or by policy.
bool IsPluginEnabled(const WebPluginInfo& plugin);

}  // namespace npapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_NPAPI_WEBPLUGININFO_H_
