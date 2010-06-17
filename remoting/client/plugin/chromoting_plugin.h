// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_CHROMOTING_PLUGIN_H_
#define REMOTING_CLIENT_PLUGIN_CHROMOTING_PLUGIN_H_

#include <string>

#include "base/at_exit.h"
#include "base/scoped_ptr.h"
#include "remoting/client/host_connection.h"
#include "remoting/client/pepper/pepper_plugin.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace base {
class Thread;
}  // namespace base

namespace remoting {

class ChromotingClient;
class HostConnection;
class JingleThread;
class PepperView;

class ChromotingClient;

class ChromotingPlugin : public pepper::PepperPlugin {
 public:
  // The mimetype for which this plugin is registered.
  //
  // TODO(ajwong): Mimetype doesn't really make sense for us as the trigger
  // point.  I think we should handle a special protocol (eg., chromotocol://)
  static const char *kMimeType;

  ChromotingPlugin(NPNetscapeFuncs* browser_funcs, NPP instance);
  virtual ~ChromotingPlugin();

  // PepperPlugin implementation.
  virtual NPError New(NPMIMEType pluginType, int16 argc, char* argn[],
                      char* argv[]);
  virtual NPError Destroy(NPSavedData** save);
  virtual NPError SetWindow(NPWindow* window);
  virtual int16 HandleEvent(void* event);
  virtual NPError GetValue(NPPVariable variable, void* value);
  virtual NPError SetValue(NPNVariable variable, void* value);

 private:
  FRIEND_TEST(ChromotingPluginTest, ParseUrl);
  FRIEND_TEST(ChromotingPluginTest, TestCaseSetup);

  NPDevice* device() { return device_; }

  static bool ParseUrl(const std::string& url,
                       std::string* user_id,
                       std::string* auth_token,
                       std::string* host_jid);

  // Size of the plugin window.
  int width_, height_;

  // Rendering device provided by browser.
  NPDevice* device_;

  // Since the ChromotingPlugin's lifetime is conceptually the lifetime of the
  // object, use it to control running of atexit() calls.
  //
  // TODO(ajwong): Should this be moved into PepperPlugin?  Or maybe even
  // higher?
  //
  // TODO(ajwong): This causes unittests to fail so commenting out for now. We
  // need to understand process instantiation better.  This may also be a
  // non-issue when we are no longer being loaded as a DSO.
  //
  // base::AtExitManager at_exit_manager_;

  scoped_ptr<base::Thread> main_thread_;
  scoped_ptr<JingleThread> network_thread_;

  scoped_ptr<HostConnection> host_connection_;
  scoped_ptr<PepperView> view_;
  scoped_ptr<ChromotingClient> client_;

  DISALLOW_COPY_AND_ASSIGN(ChromotingPlugin);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_CHROMOTING_PLUGIN_H_
