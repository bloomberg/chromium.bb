// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "o3d/gpu_plugin/gpu_plugin.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/plugins/nphostapi.h"

#if defined(OS_LINUX)
#define INITIALIZE_PLUGIN_FUNCS , &plugin_funcs_
#else
#define INITIALIZE_PLUGIN_FUNCS
#endif

namespace o3d {
namespace gpu_plugin {

class GPUPluginTest : public testing::Test {
 protected:
  virtual void SetUp() {
    memset(&browser_funcs_, 0, sizeof(browser_funcs_));
    memset(&plugin_funcs_, 0, sizeof(plugin_funcs_));
  }

  NPNetscapeFuncs browser_funcs_;
  NPPluginFuncs plugin_funcs_;
};

TEST_F(GPUPluginTest, GetEntryPointsSetsNeededFunctionPointers) {
#if defined(OS_LINUX)
  NPError error = NP_Initialize(&browser_funcs_, &plugin_funcs_);
  NP_Shutdown();
#else
  NPError error = NP_GetEntryPoints(&plugin_funcs_);
#endif

  EXPECT_EQ(NPERR_NO_ERROR, error);
  EXPECT_TRUE(NULL != plugin_funcs_.newp);
  EXPECT_TRUE(NULL != plugin_funcs_.destroy);
  EXPECT_TRUE(NULL != plugin_funcs_.setwindow);
  EXPECT_TRUE(NULL != plugin_funcs_.event);
  EXPECT_TRUE(NULL != plugin_funcs_.getvalue);
  EXPECT_TRUE(NULL != plugin_funcs_.setvalue);
}

TEST_F(GPUPluginTest, CanInitializeAndShutdownPlugin) {
  EXPECT_EQ(NPERR_NO_ERROR,
      NP_Initialize(&browser_funcs_ INITIALIZE_PLUGIN_FUNCS));
  EXPECT_EQ(NPERR_NO_ERROR, NP_Shutdown());
}

TEST_F(GPUPluginTest, InitializeFailsIfBrowserFuncsIsNull) {
  EXPECT_EQ(NPERR_INVALID_FUNCTABLE_ERROR,
      NP_Initialize(NULL INITIALIZE_PLUGIN_FUNCS));
}

TEST_F(GPUPluginTest, InitializeFailsIfAlreadyInitialized) {
  EXPECT_EQ(NPERR_NO_ERROR,
      NP_Initialize(&browser_funcs_ INITIALIZE_PLUGIN_FUNCS));
  EXPECT_EQ(NPERR_GENERIC_ERROR,
      NP_Initialize(&browser_funcs_ INITIALIZE_PLUGIN_FUNCS));
  EXPECT_EQ(NPERR_NO_ERROR, NP_Shutdown());
}

TEST_F(GPUPluginTest, ShutdownFailsIfNotInitialized) {
  EXPECT_EQ(NPERR_GENERIC_ERROR, NP_Shutdown());
}

}  // namespace gpu_plugin
}  // namespace o3d
