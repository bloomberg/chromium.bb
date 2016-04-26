// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/certificate_watcher.h"

#include <cstdlib>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

const char kWatchFileName[] = "testfile.txt";

const int kMessageLoopWaitMsecs = 150;

class CertificateWatcherTest : public testing::Test {
 public:
  CertificateWatcherTest()
      : task_runner_(message_loop_.task_runner()),
        watch_path_(CreateAndGetUniqueTempDir().AppendASCII(kWatchFileName)) {
    watcher_.reset(new CertificateWatcher(
        base::Bind(&CertificateWatcherTest::OnRestart,
        base::Unretained(this)),
        task_runner_));
    watcher_->SetDelayForTests(base::TimeDelta::FromSeconds(0));
    watcher_->SetWatchPathForTests(watch_path_);
  }

  ~CertificateWatcherTest() override {
    watcher_.reset();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  // Call this if you expect a restart after the loop runs.
  // The thread will hang if OnRestart is never called.
  void RunLoop() {
    base::RunLoop loop;
    quit_loop_closure_ = loop.QuitClosure();
    loop.Run();
  }

  // Call this if you expect no restart after the loop runs.
  // Will quit the loop after kMessageLoopWaitMsecs.
  void RunAndWait() {
    base::RunLoop loop;
    task_runner_->PostDelayedTask(FROM_HERE,loop.QuitClosure(),
                                  loop_wait_);
    loop.Run();
  }

  void Start() {
    watcher_->Start();
  }

  void Connect() {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&CertificateWatcher::OnClientConnected,
                   base::Unretained(watcher_.get()), ""));
  }

  void Disconnect() {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&CertificateWatcher::OnClientDisconnected,
                   base::Unretained(watcher_.get()), ""));
  }

  void TouchFile() {
    task_runner_->PostTask(
              FROM_HERE, base::Bind(&CertificateWatcherTest::TouchFileTask,
                                    base::Unretained(this)));
  }

  void TouchFileTask() {
    std::string testWriteString = std::to_string(rand());

    base::WriteFile(watch_path_, testWriteString.c_str(),
                    testWriteString.length());
  }

  base::MessageLoopForIO message_loop_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::ScopedTempDir temp_dir_;
  base::FilePath watch_path_;
  std::unique_ptr<CertificateWatcher> watcher_;
  int restart_count_ = 0;
  base::TimeDelta loop_wait_ =
      base::TimeDelta::FromMilliseconds(kMessageLoopWaitMsecs);
  base::Closure quit_loop_closure_;

 private:
  const base::FilePath& CreateAndGetUniqueTempDir() {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    return temp_dir_.path();
  }

  void OnRestart() {
    restart_count_++;
    quit_loop_closure_.Run();
  }
};

TEST_F(CertificateWatcherTest, OneTouch) {
  EXPECT_EQ(0, restart_count_);
  Start();
  EXPECT_EQ(0, restart_count_);
  TouchFile();
  RunLoop();
  EXPECT_EQ(1, restart_count_);
}

TEST_F(CertificateWatcherTest, InhibitDeferRestart) {
  Start();
  EXPECT_EQ(0, restart_count_);
  Connect();
  EXPECT_EQ(0, restart_count_);
  TouchFile();
  RunAndWait();
  EXPECT_EQ(0, restart_count_);
  Disconnect();
  RunLoop();
  EXPECT_EQ(1, restart_count_);
}

TEST_F(CertificateWatcherTest, UninhibitAndRestart) {
  Start();
  EXPECT_EQ(0, restart_count_);
  Connect();
  EXPECT_EQ(0, restart_count_);
  Disconnect();
  RunAndWait();
  EXPECT_EQ(0, restart_count_);
  TouchFile();
  RunLoop();
  EXPECT_EQ(1, restart_count_);
}

} // namespace remoting
