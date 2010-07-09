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
#include "third_party/ppapi/c/pp_event.h"
#include "third_party/ppapi/c/pp_rect.h"
#include "third_party/ppapi/cpp/completion_callback.h"
#include "third_party/ppapi/cpp/image_data.h"

using std::string;
using std::vector;

namespace remoting {

const char* ChromotingPlugin::kMimeType = "pepper-application/x-chromoting";

ChromotingPlugin::ChromotingPlugin(PP_Instance pp_instance,
                                   const PPB_Instance* ppb_instance_funcs)
    : width_(0),
      height_(0),
      drawing_context_(NULL),
      pp_instance_(pp_instance),
      ppb_instance_funcs_(ppb_instance_funcs) {
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
  /*
  view_.reset(new PepperView(main_thread_->message_loop(), device_,
                             instance()));
                             */
  //client_.reset(new ChromotingClient(main_thread_->message_loop(),
  //                                   host_connection_.get(), view_.get()));

  // Kick off the connection.
  //host_connection_->Connect(user_id, auth_token, host_jid, client_.get());

  return true;
}

void ChromotingPlugin::ViewChanged(const PP_Rect& position,
                                   const PP_Rect& clip) {
  // TODO(ajwong): This is going to be a race condition when the view changes
  // and we're in the middle of a Paint().
  LOG(INFO) << "ViewChanged "
            << position.point.x << ","
            << position.point.y << ","
            << position.size.width << ","
            << position.size.height;

  // TODO(ajwong): Do we care about the position?  Probably not...
  if (position.size.width == width_ || position.size.height == height_)
    return;

  width_ = position.size.width;
  height_ = position.size.height;

  /*
   * TODO(ajwong): Reenable this code once we fingure out how we want to
   * abstract away the C-api for DeviceContext2D.
  device_context_ = pp::DeviceContext2D(width_, height_, false);
  if (!ppb_instance_funcs_->BindGraphicsDeviceContext(
      pp_instance_,
      device_context_.pp_resource())) {
    LOG(ERROR) << "Couldn't bind the device context.";
    return;
  }

  pp::ImageData image(PP_IMAGEDATAFORMAT_BGRA_PREMUL, width_, height_, false);
  if (!image.is_null()) {
    for (int y = 0; y < image.height(); y++) {
      for (int x = 0; x < image.width(); x++) {
        *image.GetAddr32(x, y) = 0xccff00cc;
      }
    }
    device_context_.ReplaceContents(&image);
    device_context_.Flush(pp::CompletionCallback(NULL, this));
  } else {
    LOG(ERROR) << "Unable to allocate image.";
  }
  */

  //client_->SetViewport(0, 0, width_, height_);
  //client_->Repaint();
}

bool ChromotingPlugin::HandleEvent(const PP_Event& event) {
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
