// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(ajwong): We need to come up with a better description of the
// responsibilities for each thread.

#ifndef REMOTING_CLIENT_PLUGIN_CHROMOTING_INSTANCE_H_
#define REMOTING_CLIENT_PLUGIN_CHROMOTING_INSTANCE_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/scoped_ptr.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/var.h"
#include "remoting/client/client_context.h"
#include "remoting/client/plugin/chromoting_scriptable_object.h"
#include "remoting/protocol/connection_to_host.h"

class MessageLoop;
struct PP_InputEvent;

namespace base {
class Thread;
}  // namespace base

namespace pp {
class Module;
}  // namespace pp

namespace remoting {

namespace protocol {
class ConnectionToHost;
}  // namespace protocol

class ChromotingClient;
class ClientContext;
class InputHandler;
class JingleThread;
class PepperView;
class PepperViewProxy;
class RectangleUpdateDecoder;

struct ClientConfig;

namespace protocol {
class HostConnection;
}  // namespace protocol

class ChromotingInstance : public pp::Instance {
 public:
  // The mimetype for which this plugin is registered.
  static const char *kMimeType;

  explicit ChromotingInstance(PP_Instance instance);
  virtual ~ChromotingInstance();

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]);
  virtual void Connect(const ClientConfig& config);
  virtual void ConnectSandboxed(const std::string& your_jid,
                                const std::string& host_jid);
  virtual bool HandleInputEvent(const PP_InputEvent& event);
  virtual void Disconnect();
  virtual pp::Var GetInstanceObject();
  virtual void ViewChanged(const pp::Rect& position, const pp::Rect& clip);

  // Convenience wrapper to get the ChromotingScriptableObject.
  ChromotingScriptableObject* GetScriptableObject();

  // Called by ChromotingScriptableObject to provide username and password.
  void SubmitLoginInfo(const std::string& username,
                       const std::string& password);

  void LogDebugInfo(const std::string& info);

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromotingInstanceTest, TestCaseSetup);

  bool initialized_;

  ClientContext context_;
  scoped_ptr<protocol::ConnectionToHost> host_connection_;
  scoped_ptr<PepperView> view_;

  // PepperViewProxy is refcounted and used to interface between chromoting
  // objects and PepperView and perform thread switching. It wraps around
  // |view_| and receives method calls on chromoting threads. These method
  // calls are then delegates on the pepper thread. During destruction of
  // ChromotingInstance we need to detach PepperViewProxy from PepperView since
  // both ChromotingInstance and PepperView are destroyed and there will be
  // outstanding tasks on the pepper message loo.
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

  DISALLOW_COPY_AND_ASSIGN(ChromotingInstance);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_CHROMOTING_INSTANCE_H_
