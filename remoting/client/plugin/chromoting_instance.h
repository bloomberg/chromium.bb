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
  virtual bool HandleInputEvent(const PP_InputEvent& event);
  virtual void Disconnect();
  virtual pp::Var GetInstanceObject();
  virtual void ViewChanged(const pp::Rect& position, const pp::Rect& clip);

  virtual bool CurrentlyOnPluginThread() const;

  // Convenience wrapper to get the ChromotingScriptableObject.
  ChromotingScriptableObject* GetScriptableObject();

  // Called by ChromotingScriptableObject to provide username and password.
  void SubmitLoginInfo(const std::string& username,
                       const std::string& password);

  void LogDebugInfo(const std::string& info);

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromotingInstanceTest, TestCaseSetup);

  // Since we're an internal plugin, we can just grab the message loop during
  // init to figure out which thread we're on.  This should only be used to
  // sanity check which thread we're executing on. Do not post task here!
  // Instead, use PPB_Core:CallOnMainThread() in the pepper api.
  //
  // TODO(ajwong): Think if there is a better way to safeguard this.
  MessageLoop* pepper_main_loop_dont_post_to_me_;

  ClientContext context_;
  scoped_ptr<protocol::ConnectionToHost> host_connection_;
  scoped_ptr<PepperView> view_;

  // PepperViewProxy is refcounted and used to interface between shromoting
  // objects and PepperView and perform thread switching. It wraps around
  // |view_| and receives method calls on chromoting threads. These method
  // calls are then delegates on the pepper thread. During destruction of
  // ChromotingInstance we need to detach PepperViewProxy from PepperView since
  // both ChromotingInstance and PepperView are destroyed and there will be
  // outstanding tasks on the pepper message loo.
  scoped_refptr<PepperViewProxy> view_proxy_;
  scoped_ptr<RectangleUpdateDecoder> rectangle_decoder_;
  scoped_ptr<InputHandler> input_handler_;
  scoped_ptr<ChromotingClient> client_;

  // JavaScript interface to control this instance.
  // This wraps a ChromotingScriptableObject in a pp::Var.
  pp::Var instance_object_;

  DISALLOW_COPY_AND_ASSIGN(ChromotingInstance);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_CHROMOTING_INSTANCE_H_
