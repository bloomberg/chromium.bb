// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/chromoting_plugin.h"

#include <string>
#include <vector>

#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "remoting/client/chromoting_client.h"
#include "remoting/client/host_connection.h"
#include "remoting/client/jingle_host_connection.h"
#include "remoting/client/plugin/pepper_view.h"
#include "remoting/jingle_glue/jingle_thread.h"
#include "third_party/ppapi/c/pp_event.h"
#include "third_party/ppapi/c/pp_rect.h"
#include "third_party/ppapi/cpp/completion_callback.h"

using std::string;
using std::vector;

namespace remoting {

const char* ChromotingPlugin::kMimeType = "pepper-application/x-chromoting";

ChromotingPlugin::ChromotingPlugin(PP_Instance pp_instance)
    : pp::Instance(pp_instance),
      pepper_main_loop_dont_post_to_me_(NULL) {
}

ChromotingPlugin::~ChromotingPlugin() {
  if (host_connection_.get())
    host_connection_->Disconnect();

  // TODO(ajwong): We need to ensure all objects have actually stopped posting
  // to the message loop before this point.  Right now, we don't have a well
  // defined stop for the plugin process, and the thread shutdown is likely a
  // race condition.
  if (network_thread_.get())
    network_thread_->Stop();

  if (main_thread_.get())
    main_thread_->Stop();
}

bool ChromotingPlugin::Init(uint32_t argc,
                            const char* argn[],
                            const char* argv[]) {
  CHECK(pepper_main_loop_dont_post_to_me_ == NULL);

  // Record the current thread.  This function should only be invoked by the
  // plugin thread, so we capture the current message loop and assume it is
  // indeed the plugin thread.
  //
  // We're abusing the pepper API slightly here.  We know we're running as an
  // internal plugin, and thus we are on the pepper main thread that uses a
  // message loop.
  //
  // TODO(ajwong): See if there is a method for querying what thread we're on
  // from inside the pepper API.
  pepper_main_loop_dont_post_to_me_ = MessageLoop::current();
  LOG(INFO) << "Started ChromotingPlugin::Init";

  // Extract the URL from the arguments.
  const char* url = NULL;
  for (uint32_t i = 0; i < argc; ++i) {
    if (strcmp(argn[i], "src") == 0) {
      url = argv[i];
      break;
    }
  }

  if (!url) {
    return false;
  }

  string user_id;
  string auth_token;
  string host_jid;
  if (!ParseUrl(url, &user_id, &auth_token, &host_jid)) {
    LOG(WARNING) << "Could not parse URL: " << url;
    return false;
  }

  // Start the threads.
  main_thread_.reset(new base::Thread("ChromoClientMain"));
  if (!main_thread_->Start()) {
    LOG(ERROR) << "Main thread failed to start.";
    return false;
  }
  network_thread_.reset(new JingleThread());
  network_thread_->Start();

  // Create the chromting objects.
  host_connection_.reset(new JingleHostConnection(network_thread_.get()));
  view_.reset(new PepperView(this));
  client_.reset(new ChromotingClient(main_thread_->message_loop(),
                                     host_connection_.get(), view_.get()));

  // Default to a medium grey.
  view_->SetSolidFill(0xFFCDCDCD);

  // Kick off the connection.
  host_connection_->Connect(user_id, auth_token, host_jid, client_.get());

  return true;
}

void ChromotingPlugin::ViewChanged(const PP_Rect& position,
                                   const PP_Rect& clip) {
  DCHECK(CurrentlyOnPluginThread());

  // TODO(ajwong): This is going to be a race condition when the view changes
  // and we're in the middle of a Paint().
  LOG(INFO) << "ViewChanged "
            << position.point.x << ","
            << position.point.y << ","
            << position.size.width << ","
            << position.size.height;

  view_->SetViewport(position.point.x, position.point.y,
                       position.size.width, position.size.height);
  view_->Paint();
}

bool ChromotingPlugin::CurrentlyOnPluginThread() const {
  return pepper_main_loop_dont_post_to_me_ == MessageLoop::current();
}

bool ChromotingPlugin::HandleEvent(const PP_Event& event) {
  DCHECK(CurrentlyOnPluginThread());

  switch (event.type) {
    case PP_EVENT_TYPE_MOUSEDOWN:
    case PP_EVENT_TYPE_MOUSEUP:
    case PP_EVENT_TYPE_MOUSEMOVE:
    case PP_EVENT_TYPE_MOUSEENTER:
    case PP_EVENT_TYPE_MOUSELEAVE:
      //client_->handle_mouse_event(npevent);
      break;

    case PP_EVENT_TYPE_CHAR:
      //client_->handle_char_event(npevent);
      break;

    default:
      break;
  }

  return false;
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
