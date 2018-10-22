// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "extensions/browser/api/system_memory/memory_info_provider.h"
#include "extensions/shell/test/shell_apitest.h"

namespace extensions {

using api::system_memory::MemoryInfo;

class MockMemoryInfoProviderImpl : public MemoryInfoProvider {
 public:
  MockMemoryInfoProviderImpl() {}

  bool QueryInfo() override {
    info_.capacity = 4096;
    info_.available_capacity = 1024;
    return true;
  }

 private:
  ~MockMemoryInfoProviderImpl() override {}
};

class SystemMemoryApiTest : public ShellApiTest {
 public:
  SystemMemoryApiTest() {}
  ~SystemMemoryApiTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    ShellApiTest::SetUpInProcessBrowserTestFixture();
    message_loop_.reset(new base::MessageLoopForUI);
  }

 private:
  std::unique_ptr<base::MessageLoop> message_loop_;
};

IN_PROC_BROWSER_TEST_F(SystemMemoryApiTest, Memory) {
  scoped_refptr<MemoryInfoProvider> provider = new MockMemoryInfoProviderImpl();
  // The provider is owned by the single MemoryInfoProvider instance.
  MemoryInfoProvider::InitializeForTesting(provider);
  ASSERT_TRUE(RunAppTest("system/memory")) << message_;
}

}  // namespace extensions
