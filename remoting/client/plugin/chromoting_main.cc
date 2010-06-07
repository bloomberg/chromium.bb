// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/chromoting_plugin.h"

// Initialize general plugin info like name and description.
// This information needs to live outside of the PepperPlugin since it can
// be requested by the browser before the PepperPlugin has been instantiated.
void InitializePluginInfo(pepper::PepperPlugin::Info* plugin_info) {
  plugin_info->mime_description = remoting::kMimeType;
  plugin_info->plugin_name = "Chromoting";
  plugin_info->plugin_description = "Remote access for Chrome";
}

// Create the Pepper plugin instance.
//
// This is called in response to the NPP_New call.
// This instantiates a PepperPlugin subclass that implements the plugin
// specific functionality.
pepper::PepperPlugin* CreatePlugin(NPNetscapeFuncs* browser_funcs,
                                   NPP instance) {
  return new remoting::ChromotingPlugin(browser_funcs, instance);
}
