// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_CHROMOTING_PLUGIN_H_
#define REMOTING_CLIENT_PLUGIN_CHROMOTING_PLUGIN_H_

#include <string>

#include "base/at_exit.h"
#include "base/scoped_ptr.h"
#include "remoting/client/host_connection.h"
#include "testing/gtest/include/gtest/gtest_prod.h"
#include "third_party/ppapi/cpp/device_context_2d.h"
#include "third_party/ppapi/cpp/instance.h"

namespace base {
class Thread;
}  // namespace base

namespace remoting {

class ChromotingClient;
class HostConnection;
class JingleThread;
class PepperView;

class ChromotingClient;

class ChromotingPlugin : public pp::Instance {
 public:
  // The mimetype for which this plugin is registered.
  //
  // TODO(ajwong): Mimetype doesn't really make sense for us as the trigger
  // point.  I think we should handle a special protocol (eg., chromotocol://)
  static const char *kMimeType;

  ChromotingPlugin(PP_Instance instance);
  virtual ~ChromotingPlugin();

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]);
  virtual bool HandleEvent(const PP_Event& event);
  virtual void ViewChanged(const PP_Rect& position, const PP_Rect& clip);

 private:
  FRIEND_TEST(ChromotingPluginTest, ParseUrl);
  FRIEND_TEST(ChromotingPluginTest, TestCaseSetup);

  static bool ParseUrl(const std::string& url,
                       std::string* user_id,
                       std::string* auth_token,
                       std::string* host_jid);

  // Size of the plugin window.
  int width_;
  int height_;

  pp::DeviceContext2D device_context_;

  scoped_ptr<base::Thread> main_thread_;
  scoped_ptr<JingleThread> network_thread_;

  scoped_ptr<HostConnection> host_connection_;
  scoped_ptr<PepperView> view_;
  scoped_ptr<ChromotingClient> client_;

  DISALLOW_COPY_AND_ASSIGN(ChromotingPlugin);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_CHROMOTING_PLUGIN_H_
