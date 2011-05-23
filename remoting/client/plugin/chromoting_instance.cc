// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/chromoting_instance.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/task.h"
#include "base/threading/thread.h"
// TODO(sergeyu): We should not depend on renderer here. Instead P2P
// Pepper API should be used. Remove this dependency.
// crbug.com/74951
#include "content/renderer/p2p/ipc_network_manager.h"
#include "content/renderer/p2p/ipc_socket_factory.h"
#include "ppapi/c/pp_input_event.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/rect.h"
#include "remoting/client/client_config.h"
#include "remoting/client/client_util.h"
#include "remoting/client/chromoting_client.h"
#include "remoting/client/rectangle_update_decoder.h"
#include "remoting/client/plugin/chromoting_scriptable_object.h"
#include "remoting/client/plugin/pepper_client_logger.h"
#include "remoting/client/plugin/pepper_input_handler.h"
#include "remoting/client/plugin/pepper_port_allocator_session.h"
#include "remoting/client/plugin/pepper_view.h"
#include "remoting/client/plugin/pepper_view_proxy.h"
#include "remoting/client/plugin/pepper_util.h"
#include "remoting/client/plugin/pepper_xmpp_proxy.h"
#include "remoting/jingle_glue/jingle_thread.h"
#include "remoting/proto/auth.pb.h"
#include "remoting/protocol/connection_to_host.h"
// TODO(sergeyu): This is a hack: plugin should not depend on webkit
// glue. It is used here to get P2PPacketDispatcher corresponding to
// the current RenderView. Use P2P Pepper API for connection and
// remove these includes.
// crbug.com/74951
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/resource_tracker.h"

namespace remoting {

const char* ChromotingInstance::kMimeType = "pepper-application/x-chromoting";

ChromotingInstance::ChromotingInstance(PP_Instance pp_instance)
    : pp::Instance(pp_instance),
      initialized_(false),
      logger_(this) {
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
  CHECK(!initialized_);
  initialized_ = true;

  logger_.VLog(1, "Started ChromotingInstance::Init");

  // Start all the threads.
  context_.Start();

  webkit::ppapi::PluginInstance* plugin_instance =
      webkit::ppapi::ResourceTracker::Get()->GetInstance(pp_instance());

  P2PSocketDispatcher* socket_dispatcher =
      plugin_instance->delegate()->GetP2PSocketDispatcher();
  IpcNetworkManager* network_manager = NULL;
  IpcPacketSocketFactory* socket_factory = NULL;
  PortAllocatorSessionFactory* session_factory =
      CreatePepperPortAllocatorSessionFactory(this);

  // If we don't have socket dispatcher for IPC (e.g. P2P API is
  // disabled), then JingleClient will try to use physical sockets.
  if (socket_dispatcher) {
    logger_.VLog(1, "Creating IpcNetworkManager and IpcPacketSocketFactory.");
    network_manager = new IpcNetworkManager(socket_dispatcher);
    socket_factory = new IpcPacketSocketFactory(socket_dispatcher);
  }

  // Create the chromoting objects.
  host_connection_.reset(new protocol::ConnectionToHost(
      context_.jingle_thread(), network_manager, socket_factory,
      session_factory));
  view_.reset(new PepperView(this, &context_));
  view_proxy_ = new PepperViewProxy(this, view_.get());
  rectangle_decoder_ = new RectangleUpdateDecoder(
      context_.decode_message_loop(), view_proxy_);
  input_handler_.reset(new PepperInputHandler(&context_,
                                              host_connection_.get(),
                                              view_proxy_));

  // Default to a medium grey.
  view_->SetSolidFill(0xFFCDCDCD);

  return true;
}

void ChromotingInstance::Connect(const ClientConfig& config) {
  DCHECK(CurrentlyOnPluginThread());

  logger_.Log(logging::LOG_INFO, "Connecting to %s as %s",
               config.host_jid.c_str(),
               config.username.c_str());
  client_.reset(new ChromotingClient(config,
                                     &context_,
                                     host_connection_.get(),
                                     view_proxy_,
                                     rectangle_decoder_.get(),
                                     input_handler_.get(),
                                     &logger_,
                                     NULL));

  // Kick off the connection.
  client_->Start();

  logger_.Log(logging::LOG_INFO, "Connection status: Initializing");
  GetScriptableObject()->SetConnectionInfo(STATUS_INITIALIZING,
                                           QUALITY_UNKNOWN);
}

void ChromotingInstance::ConnectSandboxed(const std::string& your_jid,
                                          const std::string& host_jid,
                                          const std::string& nonce) {
  // TODO(ajwong): your_jid and host_jid should be moved into ClientConfig. In
  // fact, this whole function should go away, and Connect() should just look at
  // ClientConfig.
  DCHECK(CurrentlyOnPluginThread());

  logger_.Log(logging::LOG_INFO, "Attempting sandboxed connection.");

  // Setup the XMPP Proxy.
  ChromotingScriptableObject* scriptable_object = GetScriptableObject();
  scoped_refptr<PepperXmppProxy> xmpp_proxy =
      new PepperXmppProxy(scriptable_object->AsWeakPtr(),
                          context_.jingle_thread()->message_loop());
  scriptable_object->AttachXmppProxy(xmpp_proxy);

  ClientConfig config_;
  config_.nonce = nonce;
  client_.reset(new ChromotingClient(config_,
                                     &context_,
                                     host_connection_.get(),
                                     view_proxy_,
                                     rectangle_decoder_.get(),
                                     input_handler_.get(),
                                     &logger_,
                                     NULL));

  // Kick off the connection.
  client_->StartSandboxed(xmpp_proxy, your_jid, host_jid);

  GetScriptableObject()->SetConnectionInfo(STATUS_INITIALIZING,
                                           QUALITY_UNKNOWN);
}

void ChromotingInstance::Disconnect() {
  DCHECK(CurrentlyOnPluginThread());

  logger_.Log(logging::LOG_INFO, "Disconnecting from host.");
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
  logger_.VLog(1, "ViewChanged: %d,%d %dx%d",
                position.x(), position.y(),
                position.width(), position.height());

  view_->SetViewport(position.x(), position.y(),
                     position.width(), position.height());
  view_->Paint();
}

void ChromotingInstance::DidChangeView(const pp::Rect& position,
                                       const pp::Rect& clip) {
  // This lets |view_| implement scale-to-fit. But it only specifies a
  // sub-rectangle of the plugin window as the rectangle on which the host
  // screen can be displayed, so |view_| has to make sure the plugin window
  // is large.
  view_->SetScreenSize(clip.width(), clip.height());
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
      logger_.VLog(3, "PP_INPUTEVENT_TYPE_KEY%s key=%d",
                    (event.type==PP_INPUTEVENT_TYPE_KEYDOWN ? "DOWN" : "UP"),
                    event.u.key.key_code);
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
  logger_.Log(logging::LOG_ERROR,
               "Unable to get ScriptableObject for Chromoting plugin.");
  return NULL;
}

void ChromotingInstance::SubmitLoginInfo(const std::string& username,
                                         const std::string& password) {
  if (host_connection_->state() !=
      protocol::ConnectionToHost::STATE_CONNECTED) {
    logger_.Log(logging::LOG_INFO,
                 "Client not connected or already authenticated.");
    return;
  }

  protocol::LocalLoginCredentials* credentials =
      new protocol::LocalLoginCredentials();
  credentials->set_type(protocol::PASSWORD);
  credentials->set_username(username);
  credentials->set_credential(password.data(), password.length());

  host_connection_->host_stub()->BeginSessionRequest(
      credentials,
      new DeleteTask<protocol::LocalLoginCredentials>(credentials));
}

void ChromotingInstance::SetScaleToFit(bool scale_to_fit) {
  view_proxy_->SetScaleToFit(scale_to_fit);
}

void ChromotingInstance::Log(int severity, const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  logger_.va_Log(severity, format, ap);
  va_end(ap);
}

void ChromotingInstance::VLog(int verboselevel, const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  logger_.va_VLog(verboselevel, format, ap);
  va_end(ap);
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

ChromotingStats* ChromotingInstance::GetStats() {
  if (!client_.get())
    return NULL;
  return client_->GetStats();
}

}  // namespace remoting
