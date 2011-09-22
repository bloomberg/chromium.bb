// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(ajwong): We need to come up with a better description of the
// responsibilities for each thread.

#ifndef REMOTING_CLIENT_PLUGIN_CHROMOTING_INSTANCE_H_
#define REMOTING_CLIENT_PLUGIN_CHROMOTING_INSTANCE_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "ppapi/c/dev/ppp_policy_update_dev.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/private/instance_private.h"
#include "remoting/client/client_context.h"
#include "remoting/client/plugin/chromoting_scriptable_object.h"
#include "remoting/client/plugin/pepper_plugin_thread_delegate.h"
#include "remoting/protocol/connection_to_host.h"

namespace base {
class Thread;
}  // namespace base

namespace pp {
class InputEvent;
class Module;
}  // namespace pp

namespace remoting {

namespace protocol {
class ConnectionToHost;
}  // namespace protocol

class ChromotingClient;
class ChromotingStats;
class ClientContext;
class InputHandler;
class JingleThread;
class PepperView;
class PepperViewProxy;
class RectangleUpdateDecoder;
class TaskThreadProxy;

struct ClientConfig;

namespace protocol {
class HostConnection;
}  // namespace protocol

class ChromotingInstance : public pp::InstancePrivate {
 public:
  // The mimetype for which this plugin is registered.
  static const char *kMimeType;

  explicit ChromotingInstance(PP_Instance instance);
  virtual ~ChromotingInstance();

  // pp::Instance interface.
  virtual void DidChangeView(const pp::Rect& position,
                             const pp::Rect& clip) OVERRIDE;
  virtual bool Init(uint32_t argc, const char* argn[],
                    const char* argv[]) OVERRIDE;
  virtual bool HandleInputEvent(const pp::InputEvent& event) OVERRIDE;

  // pp::InstancePrivate interface.
  virtual pp::Var GetInstanceObject() OVERRIDE;

  // Convenience wrapper to get the ChromotingScriptableObject.
  ChromotingScriptableObject* GetScriptableObject();

  // Initiates and cancels connections.
  void Connect(const ClientConfig& config);
  void Disconnect();

  // Called by ChromotingScriptableObject to provide username and password.
  void SubmitLoginInfo(const std::string& username,
                       const std::string& password);

  // Called by ChromotingScriptableObject to set scale-to-fit.
  void SetScaleToFit(bool scale_to_fit);

  // Return statistics record by ChromotingClient.
  // If no connection is currently active then NULL will be returned.
  ChromotingStats* GetStats();

  void ReleaseAllKeys();

  bool DoScaling() const { return scale_to_fit_; }

  // Registers a global log message handler that redirects the log output to
  // our plugin instance.
  // This is called by the plugin's PPP_InitializeModule.
  // Note that no logging will be processed unless a ChromotingInstance has been
  // registered for logging (see RegisterLoggingInstance).
  static void RegisterLogMessageHandler();

  // Registers this instance so it processes messages sent by the global log
  // message handler. This overwrites any previously registered instance.
  void RegisterLoggingInstance();

  // Unregisters this instance so that debug log messages will no longer be sent
  // to it. If this instance is not the currently registered logging instance,
  // then the currently registered instance will stay in effect.
  void UnregisterLoggingInstance();

  // A Log Message Handler that is called after each LOG message has been
  // processed. This must be of type LogMessageHandlerFunction defined in
  // base/logging.h.
  static bool LogToUI(int severity, const char* file, int line,
                      size_t message_start, const std::string& str);
 private:
  FRIEND_TEST_ALL_PREFIXES(ChromotingInstanceTest, TestCaseSetup);

  static PPP_PolicyUpdate_Dev kPolicyUpdatedInterface;
  static void PolicyUpdatedThunk(PP_Instance pp_instance,
                                 PP_Var pp_policy_json);
  void SubscribeToNatTraversalPolicy();
  bool IsNatTraversalAllowed(const std::string& policy_json);
  void HandlePolicyUpdate(const std::string policy_json);

  void ProcessLogToUI(const std::string& message);

  bool initialized_;

  PepperPluginThreadDelegate plugin_thread_delegate_;
  scoped_refptr<PluginMessageLoopProxy> plugin_message_loop_;
  ClientContext context_;
  scoped_ptr<protocol::ConnectionToHost> host_connection_;
  scoped_ptr<PepperView> view_;

  // True if scale to fit is enabled.
  bool scale_to_fit_;

  // A refcounted class to perform thread-switching for logging tasks.
  scoped_refptr<TaskThreadProxy> log_proxy_;

  // PepperViewProxy is refcounted and used to interface between chromoting
  // objects and PepperView and perform thread switching. It wraps around
  // |view_| and receives method calls on chromoting threads. These method
  // calls are then delegates on the pepper thread. During destruction of
  // ChromotingInstance we need to detach PepperViewProxy from PepperView since
  // both ChromotingInstance and PepperView are destroyed and there will be
  // outstanding tasks on the pepper message loop.
  scoped_refptr<PepperViewProxy> view_proxy_;
  scoped_refptr<RectangleUpdateDecoder> rectangle_decoder_;
  scoped_ptr<InputHandler> input_handler_;
  scoped_ptr<ChromotingClient> client_;

  // XmppProxy is a refcounted interface used to perform thread-switching and
  // detaching between objects whose lifetimes are controlled by pepper, and
  // jingle_glue objects. This is used when if we start a sandboxed jingle
  // connection.
  scoped_refptr<PepperXmppProxy> xmpp_proxy_;

  // JavaScript interface to control this instance.
  // This wraps a ChromotingScriptableObject in a pp::Var.
  pp::Var instance_object_;

  // Controls if this instance of the plugin should attempt to bridge
  // firewalls.
  bool enable_client_nat_traversal_;

  // True when the initial policy is received. Used to avoid taking
  // action before the browser has informed the plugin about its policy
  // settings.
  bool initial_policy_received_;

  ScopedRunnableMethodFactory<ChromotingInstance> task_factory_;
  scoped_ptr<Task> delayed_connect_;

  DISALLOW_COPY_AND_ASSIGN(ChromotingInstance);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_CHROMOTING_INSTANCE_H_
