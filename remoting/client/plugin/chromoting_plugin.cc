// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/chromoting_plugin.h"

#include <string>
#include <vector>

#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "remoting/client/client_config.h"
#include "remoting/client/client_util.h"
#include "remoting/client/chromoting_client.h"
#include "remoting/client/host_connection.h"
#include "remoting/client/jingle_host_connection.h"
#include "remoting/client/plugin/chromoting_scriptable_object.h"
#include "remoting/client/plugin/pepper_input_handler.h"
#include "remoting/client/plugin/pepper_view.h"
#include "remoting/jingle_glue/jingle_thread.h"
#include "third_party/ppapi/c/pp_event.h"
#include "third_party/ppapi/cpp/completion_callback.h"
#include "third_party/ppapi/cpp/rect.h"

namespace remoting {

const char* ChromotingPlugin::kMimeType = "pepper-application/x-chromoting";

ChromotingPlugin::ChromotingPlugin(PP_Instance pp_instance)
    : pp::Instance(pp_instance),
      pepper_main_loop_dont_post_to_me_(NULL) {
}

ChromotingPlugin::~ChromotingPlugin() {
  if (client_.get()) {
    client_->Stop();
  }

  // TODO(ajwong): We need to ensure all objects have actually stopped posting
  // to the message loop before this point.  Right now, we don't have a well
  // defined stop for the plugin process, and the thread shutdown is likely a
  // race condition.
  context_.Stop();
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

  // Start all the threads.
  context_.Start();

  // Create the chromoting objects.
  host_connection_.reset(new JingleHostConnection(&context_));
  view_.reset(new PepperView(this));
  input_handler_.reset(new PepperInputHandler());

  // Default to a medium grey.
  view_->SetSolidFill(0xFFCDCDCD);

  return true;
}

void ChromotingPlugin::Connect(const ClientConfig& config) {
  DCHECK(CurrentlyOnPluginThread());

  client_.reset(new ChromotingClient(config,
                                     &context_,
                                     host_connection_.get(),
                                     view_.get(),
                                     input_handler_.get(),
                                     NULL));

  // Kick off the connection.
  client_->Start();
}

void ChromotingPlugin::ViewChanged(const pp::Rect& position,
                                   const pp::Rect& clip) {
  DCHECK(CurrentlyOnPluginThread());

  // TODO(ajwong): This is going to be a race condition when the view changes
  // and we're in the middle of a Paint().
  LOG(INFO) << "ViewChanged "
            << position.x() << ","
            << position.y() << ","
            << position.width() << ","
            << position.height();

  view_->SetViewport(position.x(), position.y(),
                     position.width(), position.height());
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

pp::Var ChromotingPlugin::GetInstanceObject() {
  LOG(ERROR) << "Getting instance object.";
  if (instance_object_.is_void()) {
    ChromotingScriptableObject* object = new ChromotingScriptableObject(this);
    object->Init();

    LOG(ERROR) << "Object initted.";
    // The pp::Var takes ownership of object here.
    instance_object_ = pp::Var(object);
  }

  return instance_object_;
}

}  // namespace remoting
