// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/chromoting_instance.h"

#include <string>
#include <vector>

#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/threading/thread.h"
#include "ppapi/c/pp_input_event.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/rect.h"
#include "remoting/client/client_config.h"
#include "remoting/client/client_util.h"
#include "remoting/client/chromoting_client.h"
#include "remoting/client/rectangle_update_decoder.h"
#include "remoting/client/plugin/chromoting_scriptable_object.h"
#include "remoting/client/plugin/pepper_input_handler.h"
#include "remoting/client/plugin/pepper_view.h"
#include "remoting/client/plugin/pepper_view_proxy.h"
#include "remoting/jingle_glue/jingle_thread.h"
#include "remoting/proto/auth.pb.h"
#include "remoting/protocol/connection_to_host.h"
#include "remoting/protocol/jingle_connection_to_host.h"

namespace remoting {

const char* ChromotingInstance::kMimeType = "pepper-application/x-chromoting";

ChromotingInstance::ChromotingInstance(PP_Instance pp_instance)
    : pp::Instance(pp_instance),
      pepper_main_loop_dont_post_to_me_(NULL) {
}

ChromotingInstance::~ChromotingInstance() {
  if (client_.get()) {
    client_->Stop();
  }

  // Stopping the context shutdown all chromoting threads. This is a requirement
  // before we can call Detach() on |view_proxy_|.
  context_.Stop();

  view_proxy_->Detach();
}

bool ChromotingInstance::Init(uint32_t argc,
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
  VLOG(1) << "Started ChromotingInstance::Init";

  // Start all the threads.
  context_.Start();

  // Create the chromoting objects.
  host_connection_.reset(new protocol::JingleConnectionToHost(
      context_.jingle_thread()));
  view_.reset(new PepperView(this, &context_));
  view_proxy_ = new PepperViewProxy(this, view_.get());
  rectangle_decoder_.reset(
      new RectangleUpdateDecoder(context_.decode_message_loop(),
                                 view_proxy_));
  input_handler_.reset(new PepperInputHandler(&context_,
                                              host_connection_.get(),
                                              view_proxy_));

  // Default to a medium grey.
  view_->SetSolidFill(0xFFCDCDCD);

  return true;
}

void ChromotingInstance::Connect(const ClientConfig& config) {
  DCHECK(CurrentlyOnPluginThread());

  client_.reset(new ChromotingClient(config,
                                     &context_,
                                     host_connection_.get(),
                                     view_proxy_,
                                     rectangle_decoder_.get(),
                                     input_handler_.get(),
                                     NULL));

  // Kick off the connection.
  client_->Start();

  GetScriptableObject()->SetConnectionInfo(STATUS_INITIALIZING,
                                           QUALITY_UNKNOWN);
}

void ChromotingInstance::Disconnect() {
  DCHECK(CurrentlyOnPluginThread());

  if (client_.get()) {
    client_->Stop();
  }

  GetScriptableObject()->SetConnectionInfo(STATUS_CLOSED, QUALITY_UNKNOWN);
}

void ChromotingInstance::ViewChanged(const pp::Rect& position,
                                     const pp::Rect& clip) {
  DCHECK(CurrentlyOnPluginThread());

  // TODO(ajwong): This is going to be a race condition when the view changes
  // and we're in the middle of a Paint().
  VLOG(1) << "ViewChanged " << position.x() << "," << position.y() << ","
          << position.width() << "," << position.height();

  view_->SetViewport(position.x(), position.y(),
                     position.width(), position.height());
  view_->Paint();
}

bool ChromotingInstance::CurrentlyOnPluginThread() const {
  return pepper_main_loop_dont_post_to_me_ == MessageLoop::current();
}

bool ChromotingInstance::HandleInputEvent(const PP_InputEvent& event) {
  DCHECK(CurrentlyOnPluginThread());

  PepperInputHandler* pih
      = static_cast<PepperInputHandler*>(input_handler_.get());

  switch (event.type) {
    case PP_INPUTEVENT_TYPE_MOUSEDOWN:
      pih->HandleMouseButtonEvent(true, event.u.mouse);
      return true;

    case PP_INPUTEVENT_TYPE_MOUSEUP:
      pih->HandleMouseButtonEvent(false, event.u.mouse);
      return true;

    case PP_INPUTEVENT_TYPE_MOUSEMOVE:
    case PP_INPUTEVENT_TYPE_MOUSEENTER:
    case PP_INPUTEVENT_TYPE_MOUSELEAVE:
      pih->HandleMouseMoveEvent(event.u.mouse);
      return true;

    case PP_INPUTEVENT_TYPE_CONTEXTMENU:
      // We need to return true here or else we'll get a local (plugin) context
      // menu instead of the mouseup event for the right click.
      return true;

    case PP_INPUTEVENT_TYPE_KEYDOWN:
    case PP_INPUTEVENT_TYPE_KEYUP:
      pih->HandleKeyEvent(event.type == PP_INPUTEVENT_TYPE_KEYDOWN,
                          event.u.key);
      return true;

    case PP_INPUTEVENT_TYPE_CHAR:
      pih->HandleCharacterEvent(event.u.character);
      return true;

    default:
      break;
  }

  return false;
}

ChromotingScriptableObject* ChromotingInstance::GetScriptableObject() {
  pp::Var object = GetInstanceObject();
  if (!object.is_undefined()) {
    pp::deprecated::ScriptableObject* so = object.AsScriptableObject();
    DCHECK(so != NULL);
    return static_cast<ChromotingScriptableObject*>(so);
  }
  LOG(ERROR) << "Unable to get ScriptableObject for Chromoting plugin.";
  return NULL;
}

void ChromotingInstance::SubmitLoginInfo(const std::string& username,
                                         const std::string& password) {
  protocol::LocalLoginCredentials* credentials =
      new protocol::LocalLoginCredentials();
  credentials->set_type(protocol::PASSWORD);
  credentials->set_username(username);
  credentials->set_credential(password.data(), password.length());

  host_connection_->host_stub()->BeginSessionRequest(
      credentials,
      new DeleteTask<protocol::LocalLoginCredentials>(credentials));
}

pp::Var ChromotingInstance::GetInstanceObject() {
  if (instance_object_.is_undefined()) {
    ChromotingScriptableObject* object = new ChromotingScriptableObject(this);
    object->Init();

    // The pp::Var takes ownership of object here.
    instance_object_ = pp::Var(this, object);
  }

  return instance_object_;
}

}  // namespace remoting
