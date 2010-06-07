// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "remoting/client/plugin/chromoting_plugin.h"
#include "remoting/client/pepper/fake_browser.h"
#include "testing/gtest/include/gtest/gtest.h"

// Routine to create the PepperPlugin subclass that implements all of the
// plugin-specific functionality.
pepper::PepperPlugin* CreatePlugin(NPNetscapeFuncs* browser_funcs,
                                   NPP instance);


class ChromotingPluginTest : public testing::Test {
 protected:

  virtual void SetUp() {
    // Set up the fake browser callback routines.
    fake_browser_ = Singleton<FakeBrowser>::get();
    NPNetscapeFuncs* browser_funcs_ = fake_browser_->GetBrowserFuncs();
    instance_.reset(new NPP_t());

    // Create the ChromotingPlugin for testing.
    pepper::PepperPlugin* pepper_plugin;
    pepper_plugin = CreatePlugin(browser_funcs_, instance_.get());
    plugin_.reset(
        reinterpret_cast<remoting::ChromotingPlugin*>(pepper_plugin));
  }

  virtual void TearDown() {
  }

  FakeBrowser* fake_browser_;
  scoped_ptr<NPP_t> instance_;
  scoped_ptr<remoting::ChromotingPlugin> plugin_;
};

TEST_F(ChromotingPluginTest, TestSetup) {
  ASSERT_TRUE(plugin_->browser() != NULL);
  ASSERT_TRUE(plugin_->extensions() != NULL);
  ASSERT_TRUE(plugin_->instance() != NULL);

  ASSERT_TRUE(plugin_->device() != NULL);
}

TEST_F(ChromotingPluginTest, TestNew) {
  NPMIMEType mimetype =
      const_cast<NPMIMEType>("pepper-application/x-chromoting-plugin");
  int16 argc;
  char* argn[4];
  char* argv[4];
  NPError result;

  // Test 0 arguments (NULL arrays).
  argc = 0;
  result = plugin_->New(mimetype, argc, NULL, NULL);
  ASSERT_EQ(NPERR_GENERIC_ERROR, result);

  // Test 0 arguments.
  argc = 0;
  result = plugin_->New(mimetype, argc, argn, argv);
  ASSERT_EQ(NPERR_GENERIC_ERROR, result);

  // Test 1 argument (missing "src").
  argc = 1;
  argn[0] = const_cast<char*>("noturl");
  argv[0] = const_cast<char*>("random.value");
  result = plugin_->New(mimetype, argc, argn, argv);
  ASSERT_EQ(NPERR_GENERIC_ERROR, result);

  // Test "src" argument.
  argc = 1;
  argn[0] = const_cast<char*>("src");
  argv[0] = const_cast<char*>("chromotocol:name@chromoting.org");
  result = plugin_->New(mimetype, argc, argn, argv);
  ASSERT_EQ(NPERR_NO_ERROR, result);
}


static uint32 get_pixel(uint32* pixels, int stride, int x, int y) {
  return pixels[((x) + ((y) * (stride >> 2)))];
}

TEST_F(ChromotingPluginTest, TestSetWindow) {
  NPWindow* window = fake_browser_->GetWindow();
  NPError result;

  result = plugin_->SetWindow(window);
  ASSERT_EQ(NPERR_NO_ERROR, result);
}
