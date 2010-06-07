// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_CHROMOTING_PLUGIN_H_
#define REMOTING_CLIENT_PLUGIN_CHROMOTING_PLUGIN_H_

#include <string>

#include "remoting/client/pepper/pepper_plugin.h"

namespace remoting {

static const char kMimeType[]
    = "pepper-application/x-chromoting-plugin::Chromoting";

class ChromotingClient;

class ChromotingPlugin : public pepper::PepperPlugin {
 public:
  ChromotingPlugin(NPNetscapeFuncs* browser_funcs, NPP instance);
  virtual ~ChromotingPlugin();

  int width() { return width_; }
  int height() { return height_; }
  NPDevice* device() { return device_; }

  NPError New(NPMIMEType pluginType, int16 argc, char* argn[], char* argv[]);
  NPError Destroy(NPSavedData** save);
  NPError SetWindow(NPWindow* window);
  int16 HandleEvent(void* event);
  NPError GetValue(NPPVariable variable, void* value);
  NPError SetValue(NPNVariable variable, void* value);

  // Set up drawing context and update display.
  void draw();

 private:
  // Size of the plugin window.
  int width_, height_;

  // Rendering device provided by browser.
  NPDevice* device_;

  // Chromoting client manager.
  ChromotingClient* client_;

  DISALLOW_COPY_AND_ASSIGN(ChromotingPlugin);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_CHROMOTING_PLUGIN_H_
