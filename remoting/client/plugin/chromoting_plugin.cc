// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/chromoting_plugin.h"

#include "remoting/client/plugin/client.h"

namespace remoting {

ChromotingPlugin::ChromotingPlugin(NPNetscapeFuncs* browser_funcs,
                                   NPP instance)
    : PepperPlugin(browser_funcs, instance) {
  device_ = extensions()->acquireDevice(instance, NPPepper2DDevice);
  client_ = new ChromotingClient(this);
}

ChromotingPlugin::~ChromotingPlugin() {
}

NPError ChromotingPlugin::New(NPMIMEType pluginType,
                              int16 argc, char* argn[], char* argv[]) {
  // Verify the mime type and subtype
  std::string mime(kMimeType);
  std::string::size_type type_end = mime.find("/");
  std::string::size_type subtype_end = mime.find(":", type_end);
  if (strncmp(pluginType, kMimeType, subtype_end)) {
    return NPERR_GENERIC_ERROR;
  }

  // Extract the URL from the arguments.
  char* url = NULL;
  for (int i = 0; i < argc; ++i) {
    if (strcmp(argn[i], "src") == 0) {
      url = argv[i];
      break;
    }
  }

  if (!url) {
    return NPERR_GENERIC_ERROR;
  }

  return NPERR_NO_ERROR;
}

NPError ChromotingPlugin::Destroy(NPSavedData** save) {
  return NPERR_NO_ERROR;
}

void ChromotingPlugin::draw() {
  NPDeviceContext2D context;
  NPDeviceContext2DConfig config;
  device_->initializeContext(instance(), &config, &context);

  client_->draw(width_, height_, &context);

  device_->flushContext(instance(), &context, NULL, NULL);
}

NPError ChromotingPlugin::SetWindow(NPWindow* window) {
  width_ = window->width;
  height_ = window->height;

  client_->set_window();

  draw();

  return NPERR_NO_ERROR;
}

int16 ChromotingPlugin::HandleEvent(void* event) {
  NPPepperEvent* npevent = static_cast<NPPepperEvent*>(event);
  char ch;

  switch (npevent->type) {
    case NPEventType_MouseDown:
      // Fall through
    case NPEventType_MouseUp:
      // Fall through
    case NPEventType_MouseMove:
      // Fall through
    case NPEventType_MouseEnter:
      // Fall through
    case NPEventType_MouseLeave:
      client_->handle_mouse_event(npevent);
      break;
    case NPEventType_MouseWheel:
    case NPEventType_RawKeyDown:
      break;
    case NPEventType_KeyDown:
    case NPEventType_KeyUp:
      break;
    case NPEventType_Char:
      client_->handle_char_event(npevent);
      break;
    case NPEventType_Minimize:
    case NPEventType_Focus:
    case NPEventType_Device:
      break;
  }

  return false;
}

NPError ChromotingPlugin::GetValue(NPPVariable variable, void* value) {
  return NPERR_NO_ERROR;
}

NPError ChromotingPlugin::SetValue(NPNVariable variable, void* value) {
  return NPERR_NO_ERROR;
}

}  // namespace remoting
