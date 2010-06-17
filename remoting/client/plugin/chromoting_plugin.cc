// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/chromoting_plugin.h"

#include <string>
#include <vector>

#include "base/string_util.h"
#include "base/thread.h"
#include "remoting/client/chromoting_client.h"
#include "remoting/client/host_connection.h"
#include "remoting/client/jingle_host_connection.h"
#include "remoting/client/plugin/pepper_view.h"
#include "remoting/jingle_glue/jingle_thread.h"

using std::string;
using std::vector;

namespace remoting {

const char* ChromotingPlugin::kMimeType =
    "pepper-application/x-chromoting-plugin::Chromoting";

ChromotingPlugin::ChromotingPlugin(NPNetscapeFuncs* browser_funcs,
                                   NPP instance)
    : PepperPlugin(browser_funcs, instance) {
}

ChromotingPlugin::~ChromotingPlugin() {
}

NPError ChromotingPlugin::New(NPMIMEType pluginType,
                              int16 argc, char* argn[], char* argv[]) {
  LOG(INFO) << "Started ChromotingPlugin::New";

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

  string user_id;
  string auth_token;
  string host_jid;
  if (!ParseUrl(url, &user_id, &auth_token, &host_jid)) {
    LOG(WARNING) << "Could not parse URL: " << url;
    return NPERR_GENERIC_ERROR;
  }

  // Setup pepper context.
  device_ = extensions()->acquireDevice(instance(), NPPepper2DDevice);

  // Start the threads.
  main_thread_.reset(new base::Thread("ChromoClientMain"));
  if (!main_thread_->Start()) {
    LOG(ERROR) << "Main thread failed to start.";
    return NPERR_GENERIC_ERROR;
  }
  network_thread_.reset(new JingleThread());
  network_thread_->Start();

  // Create the chromting objects.
  host_connection_.reset(new JingleHostConnection(network_thread_.get()));
  view_.reset(new PepperView(main_thread_->message_loop(), device_,
                             instance()));
  client_.reset(new ChromotingClient(main_thread_->message_loop(),
                                     host_connection_.get(), view_.get()));

  // Kick off the connection.
  host_connection_->Connect(user_id, auth_token, host_jid, client_.get());

  return NPERR_NO_ERROR;
}

NPError ChromotingPlugin::Destroy(NPSavedData** save) {
  host_connection_->Disconnect();

  // TODO(ajwong): We need to ensure all objects have actually stopped posting
  // to the message loop before this point.  Right now, we don't have a well
  // defined stop for the plugin process, and the thread shutdown is likely a
  // race condition.
  network_thread_->Stop();
  main_thread_->Stop();

  main_thread_.reset();
  network_thread_.reset();
  return NPERR_NO_ERROR;
}

NPError ChromotingPlugin::SetWindow(NPWindow* window) {
  width_ = window->width;
  height_ = window->height;

  client_->SetViewport(0, 0, window->width, window->height);
  client_->Repaint();

  return NPERR_NO_ERROR;
}

int16 ChromotingPlugin::HandleEvent(void* event) {
  NPPepperEvent* npevent = static_cast<NPPepperEvent*>(event);

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
      //client_->handle_mouse_event(npevent);
      break;
    case NPEventType_MouseWheel:
    case NPEventType_RawKeyDown:
      break;
    case NPEventType_KeyDown:
    case NPEventType_KeyUp:
      break;
    case NPEventType_Char:
      //client_->handle_char_event(npevent);
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

bool ChromotingPlugin::ParseUrl(const std::string& url,
                                string* user_id,
                                string* auth_token,
                                string* host_jid) {
  // TODO(ajwong): We should use GURL or something.  Don't parse this by hand!

  // The Url should be of the form:
  //
  //   chromotocol://<hostid>?user=<userid>&auth=<authtoken>&jid=<hostjid>
  //
  vector<string> parts;
  SplitString(url, '&', &parts);
  if (parts.size() != 3) {
    return false;
  }

  size_t pos = parts[0].rfind('=');
  if (pos == string::npos && (pos + 1) != string::npos) {
    return false;
  }
  user_id->assign(parts[0].substr(pos + 1));

  pos = parts[1].rfind('=');
  if (pos == string::npos && (pos + 1) != string::npos) {
    return false;
  }
  auth_token->assign(parts[1].substr(pos + 1));

  pos = parts[2].rfind('=');
  if (pos == string::npos && (pos + 1) != string::npos) {
    return false;
  }
  host_jid->assign(parts[2].substr(pos + 1));

  return true;
}

}  // namespace remoting
