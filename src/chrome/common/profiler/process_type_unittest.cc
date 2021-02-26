// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiler/process_type.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/common/content_switches.h"
#include "extensions/buildflags/buildflags.h"
#include "sandbox/policy/switches.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/switches.h"
#endif

class ThreadProfilerProcessTypeTest : public ::testing::Test {
 public:
  ThreadProfilerProcessTypeTest()
      : command_line_(base::CommandLine::NO_PROGRAM) {}

  base::CommandLine& command_line() { return command_line_; }

 private:
  base::CommandLine command_line_;
};

TEST_F(ThreadProfilerProcessTypeTest, GetProfileParamsProcess_Browser) {
  // No process type switch == browser process.
  EXPECT_EQ(metrics::CallStackProfileParams::BROWSER_PROCESS,
            GetProfileParamsProcess(command_line()));
}

TEST_F(ThreadProfilerProcessTypeTest, GetProfileParamsProcess_Renderer) {
  command_line().AppendSwitchASCII(switches::kProcessType,
                                   switches::kRendererProcess);
  EXPECT_EQ(metrics::CallStackProfileParams::RENDERER_PROCESS,
            GetProfileParamsProcess(command_line()));
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(ThreadProfilerProcessTypeTest, GetProfileParamsProcess_Extension) {
  command_line().AppendSwitchASCII(switches::kProcessType,
                                   switches::kRendererProcess);
  command_line().AppendSwitch(extensions::switches::kExtensionProcess);
  // Extension renderers are treated separately from non-extension renderers,
  // but don't have a defined enum currently.
  EXPECT_EQ(metrics::CallStackProfileParams::UNKNOWN_PROCESS,
            GetProfileParamsProcess(command_line()));
}
#endif

TEST_F(ThreadProfilerProcessTypeTest, GetProfileParamsProcess_Gpu) {
  command_line().AppendSwitchASCII(switches::kProcessType,
                                   switches::kGpuProcess);
  EXPECT_EQ(metrics::CallStackProfileParams::GPU_PROCESS,
            GetProfileParamsProcess(command_line()));
}

TEST_F(ThreadProfilerProcessTypeTest, GetProfileParamsProcess_NetworkService) {
  command_line().AppendSwitchASCII(switches::kProcessType,
                                   switches::kUtilityProcess);
  command_line().AppendSwitchASCII(
      sandbox::policy::switches::kServiceSandboxType,
      sandbox::policy::switches::kNetworkSandbox);
  EXPECT_EQ(metrics::CallStackProfileParams::NETWORK_SERVICE_PROCESS,
            GetProfileParamsProcess(command_line()));
}

TEST_F(ThreadProfilerProcessTypeTest, GetProfileParamsProcess_Utility) {
  command_line().AppendSwitchASCII(switches::kProcessType,
                                   switches::kUtilityProcess);
  EXPECT_EQ(metrics::CallStackProfileParams::UTILITY_PROCESS,
            GetProfileParamsProcess(command_line()));
}

TEST_F(ThreadProfilerProcessTypeTest, GetProfileParamsProcess_Zygote) {
  command_line().AppendSwitchASCII(switches::kProcessType,
                                   switches::kZygoteProcess);
  EXPECT_EQ(metrics::CallStackProfileParams::ZYGOTE_PROCESS,
            GetProfileParamsProcess(command_line()));
}

TEST_F(ThreadProfilerProcessTypeTest, GetProfileParamsProcess_PpapiPlugin) {
  command_line().AppendSwitchASCII(switches::kProcessType,
                                   switches::kPpapiPluginProcess);
  EXPECT_EQ(metrics::CallStackProfileParams::PPAPI_PLUGIN_PROCESS,
            GetProfileParamsProcess(command_line()));
}
