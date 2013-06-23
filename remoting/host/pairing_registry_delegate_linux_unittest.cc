// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/pairing_registry_delegate_linux.h"

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "base/task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/timer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

using protocol::PairingRegistry;

class PairingRegistryDelegateLinuxTest : public testing::Test {
 public:
  void SaveComplete(PairingRegistry::Delegate* delegate,
                    const std::string& expected_json,
                    bool success) {
    EXPECT_TRUE(success);
    // Load the pairings again to make sure we get what we've just written.
    delegate->Load(
        base::Bind(&PairingRegistryDelegateLinuxTest::VerifyLoad,
                   base::Unretained(this),
                   expected_json));
  }

  void VerifyLoad(const std::string& expected,
                  const std::string& actual) {
    EXPECT_EQ(actual, expected);
    base::MessageLoop::current()->Quit();
  }
};

TEST_F(PairingRegistryDelegateLinuxTest, SaveAndLoad) {
  base::MessageLoop message_loop;
  base::RunLoop run_loop;

  // Create a temporary directory in order to get a unique name and use a
  // subdirectory to ensure that the AddPairing method creates the parent
  // directory if it doesn't exist.
  base::FilePath temp_dir;
  file_util::CreateNewTempDirectory("chromoting-test", &temp_dir);
  base::FilePath temp_file = temp_dir.Append("dir").Append("registry.json");

  scoped_refptr<base::TaskRunner> task_runner =
      base::ThreadTaskRunnerHandle::Get();
  scoped_ptr<PairingRegistryDelegateLinux> save_delegate(
      new PairingRegistryDelegateLinux(task_runner));
  scoped_ptr<PairingRegistryDelegateLinux> load_delegate(
      new PairingRegistryDelegateLinux(task_runner));
  save_delegate->SetFilenameForTesting(temp_file);
  load_delegate->SetFilenameForTesting(temp_file);

  // Save the pairings, then load them using a different delegate to ensure
  // that the test isn't passing due to cached values. Note that the delegate
  // doesn't require that the strings it loads and saves are valid JSON, so
  // we can simplify the test a bit.
  std::string test_data = "test data";
  save_delegate->Save(
      test_data,
      base::Bind(&PairingRegistryDelegateLinuxTest::SaveComplete,
                 base::Unretained(this),
                 load_delegate.get(),
                 test_data));

  run_loop.Run();

  file_util::Delete(temp_dir, true);
};

}  // namespace remoting
