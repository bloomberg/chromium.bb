// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/services/broker_process.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string>
#include <vector>

#if defined(OS_ANDROID)
#include "base/android/path_utils.h"
#endif
#include "base/file_path.h"
#include "base/logging.h"
#include "sandbox/linux/tests/unit_tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

TEST(BrokerProcess, CreateAndDestroy) {
  std::vector<std::string> read_whitelist;
  read_whitelist.push_back("/proc/cpuinfo");

  BrokerProcess* open_broker = new BrokerProcess(read_whitelist,
                                                 std::vector<std::string>());
  ASSERT_TRUE(open_broker->Init(NULL));
  pid_t broker_pid = open_broker->broker_pid();
  delete(open_broker);

  // Now we check that the broker has exited properly.
  int status = 0;
  EXPECT_EQ(waitpid(broker_pid, &status, 0), broker_pid);
  EXPECT_TRUE(WIFEXITED(status));
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

TEST(BrokerProcess, TestOpenNull) {
  const std::vector<std::string> empty;
  BrokerProcess open_broker(empty, empty);
  ASSERT_TRUE(open_broker.Init(NULL));

  int fd = open_broker.Open(NULL, O_RDONLY);
  EXPECT_EQ(fd, -EFAULT);
}

void TestOpenFilePerms(bool fast_check_in_client) {
  const char kR_WhiteListed[] = "/proc/DOESNOTEXIST1";
  const char kW_WhiteListed[] = "/proc/DOESNOTEXIST2";
  const char kRW_WhiteListed[] = "/proc/DOESNOTEXIST3";
  const char k_NotWhitelisted[] = "/proc/DOESNOTEXIST4";

  std::vector<std::string> read_whitelist;
  read_whitelist.push_back(kR_WhiteListed);
  read_whitelist.push_back(kRW_WhiteListed);

  std::vector<std::string> write_whitelist;
  write_whitelist.push_back(kW_WhiteListed);
  write_whitelist.push_back(kRW_WhiteListed);

  BrokerProcess open_broker(read_whitelist,
                            write_whitelist,
                            fast_check_in_client);
  ASSERT_TRUE(open_broker.Init(NULL));

  int fd = -1;
  fd = open_broker.Open(kR_WhiteListed, O_RDONLY);
  EXPECT_EQ(fd, -ENOENT);
  fd = open_broker.Open(kR_WhiteListed, O_WRONLY);
  EXPECT_EQ(fd, -EPERM);
  fd = open_broker.Open(kR_WhiteListed, O_RDWR);
  EXPECT_EQ(fd, -EPERM);

  fd = open_broker.Open(kW_WhiteListed, O_RDONLY);
  EXPECT_EQ(fd, -EPERM);
  fd = open_broker.Open(kW_WhiteListed, O_WRONLY);
  EXPECT_EQ(fd, -ENOENT);
  fd = open_broker.Open(kW_WhiteListed, O_RDWR);
  EXPECT_EQ(fd, -EPERM);

  fd = open_broker.Open(kRW_WhiteListed, O_RDONLY);
  EXPECT_EQ(fd, -ENOENT);
  fd = open_broker.Open(kRW_WhiteListed, O_WRONLY);
  EXPECT_EQ(fd, -ENOENT);
  fd = open_broker.Open(kRW_WhiteListed, O_RDWR);
  EXPECT_EQ(fd, -ENOENT);

  fd = open_broker.Open(k_NotWhitelisted, O_RDONLY);
  EXPECT_EQ(fd, -EPERM);
  fd = open_broker.Open(k_NotWhitelisted, O_WRONLY);
  EXPECT_EQ(fd, -EPERM);
  fd = open_broker.Open(k_NotWhitelisted, O_RDWR);
  EXPECT_EQ(fd, -EPERM);

  // We have some extra sanity check for clearly wrong values.
  fd = open_broker.Open(kRW_WhiteListed, O_RDONLY|O_WRONLY|O_RDWR);
  EXPECT_EQ(fd, -EPERM);

  // It makes no sense to allow O_CREAT in a 2-parameters open. Ensure this
  // is denied.
  fd = open_broker.Open(kRW_WhiteListed, O_RDWR|O_CREAT);
  EXPECT_EQ(fd, -EPERM);
}

// Run the same thing twice. The second time, we make sure that no security
// check is performed on the client.
TEST(BrokerProcess, OpenFilePermsWithClientCheck) {
  TestOpenFilePerms(true /* fast_check_in_client */);
}

TEST(BrokerProcess, OpenOpenFilePermsNoClientCheck) {
  TestOpenFilePerms(false /* fast_check_in_client */);
}


void TestOpenCpuinfo(bool fast_check_in_client) {
  const char kFileCpuInfo[] = "/proc/cpuinfo";
  std::vector<std::string> read_whitelist;
  read_whitelist.push_back(kFileCpuInfo);

  BrokerProcess* open_broker = new BrokerProcess(read_whitelist,
                                                 std::vector<std::string>(),
                                                 fast_check_in_client);
  ASSERT_TRUE(open_broker->Init(NULL));
  pid_t broker_pid = open_broker->broker_pid();

  int fd = -1;
  fd = open_broker->Open(kFileCpuInfo, O_RDWR);
  EXPECT_EQ(fd, -EPERM);

  // Open cpuinfo via the broker.
  int cpuinfo_fd = open_broker->Open(kFileCpuInfo, O_RDONLY);
  ASSERT_GE(cpuinfo_fd, 0);
  char buf[3];
  memset(buf, 0, sizeof(buf));
  int read_len1 = read(cpuinfo_fd, buf, sizeof(buf));
  EXPECT_GT(read_len1, 0);

  // Open cpuinfo directly.
  int cpuinfo_fd2 = open(kFileCpuInfo, O_RDONLY);
  ASSERT_GE(cpuinfo_fd2, 0);
  char buf2[3];
  memset(buf2, 1, sizeof(buf2));
  int read_len2 = read(cpuinfo_fd2, buf2, sizeof(buf2));
  EXPECT_GT(read_len1, 0);

  // The following is not guaranteed true, but will be in practice.
  EXPECT_EQ(read_len1, read_len2);
  // Compare the cpuinfo as returned by the broker with the one we opened
  // ourselves.
  EXPECT_EQ(memcmp(buf, buf2, read_len1), 0);

  if (fd >= 0)
    close(fd);
  if (cpuinfo_fd >= 0)
    close(cpuinfo_fd);
  if (cpuinfo_fd2 >= 0)
    close(cpuinfo_fd);

  delete(open_broker);

  // Now we check that the broker has exited properly.
  int status = 0;
  EXPECT_EQ(waitpid(broker_pid, &status, 0), broker_pid);
  EXPECT_TRUE(WIFEXITED(status));
  EXPECT_EQ(WEXITSTATUS(status), 0);
}

// Run the same thing twice. The second time, we make sure that no security
// check is performed on the client.
TEST(BrokerProcess, OpenCpuinfoWithClientCheck) {
  TestOpenCpuinfo(true /* fast_check_in_client */);
}

TEST(BrokerProcess, OpenCpuinfoNoClientCheck) {
  TestOpenCpuinfo(false /* fast_check_in_client */);
}

TEST(BrokerProcess, OpenFileRW) {
  const char basename[] = "BrokerProcessXXXXXX";
  char template_name[2048];
#if defined(OS_ANDROID)
  FilePath cache_directory;
  ASSERT_TRUE(base::android::GetCacheDirectory(&cache_directory));
  ssize_t length = snprintf(template_name, sizeof(template_name),
                            "%s%s",
                            cache_directory.value().c_str(), basename);
  ASSERT_LT(length, static_cast<ssize_t>(sizeof(template_name)));
#else
  strncpy(template_name, basename, sizeof(basename) - 1);
  template_name[sizeof(basename) - 1] = '\0';
#endif
  int tempfile = mkstemp(template_name);
  ASSERT_GE(tempfile, 0);
  char tempfile_name[2048];
  int written = snprintf(tempfile_name, sizeof(tempfile_name),
                         "/proc/self/fd/%d", tempfile);
  ASSERT_LT(written, static_cast<int>(sizeof(tempfile_name)));

  std::vector<std::string> whitelist;
  whitelist.push_back(tempfile_name);

  BrokerProcess open_broker(whitelist, whitelist);
  ASSERT_TRUE(open_broker.Init(NULL));

  int tempfile2 = -1;
  tempfile2 = open_broker.Open(tempfile_name, O_RDWR);
  ASSERT_GE(tempfile2, 0);

  // Write to the descriptor opened by the broker.
  char test_text[] = "TESTTESTTEST";
  ssize_t len = write(tempfile2, test_text, sizeof(test_text));
  ASSERT_EQ(len, static_cast<ssize_t>(sizeof(test_text)));

  // Read back from the original file descriptor what we wrote through
  // the descriptor provided by the broker.
  char buf[1024];
  len = read(tempfile, buf, sizeof(buf));

  ASSERT_EQ(len, static_cast<ssize_t>(sizeof(test_text)));
  ASSERT_EQ(memcmp(test_text, buf, sizeof(test_text)), 0);

  // Cleanup the temporary file.
  char tempfile_full_path[2048];
  // Make sure tempfile_full_path will terminate with a 0.
  memset(tempfile_full_path, 0, sizeof(tempfile_full_path));
  ssize_t ret = readlink(tempfile_name, tempfile_full_path,
                         sizeof(tempfile_full_path));
  ASSERT_GT(ret, 0);
  // Make sure we still have a trailing zero in tempfile_full_path.
  ASSERT_LT(ret, static_cast<ssize_t>(sizeof(tempfile_full_path)));
  ASSERT_EQ(unlink(tempfile_full_path), 0);

  EXPECT_EQ(close(tempfile), 0);
  EXPECT_EQ(close(tempfile2), 0);
}

// Sandbox test because we could get a SIGPIPE.
SANDBOX_TEST(BrokerProcess, BrokerDied) {
  std::vector<std::string> read_whitelist;
  read_whitelist.push_back("/proc/cpuinfo");

  BrokerProcess open_broker(read_whitelist,
                            std::vector<std::string>(),
                            true /* fast_check_in_client */,
                            true /* quiet_failures_for_tests */);
  SANDBOX_ASSERT(open_broker.Init(NULL));
  pid_t broker_pid = open_broker.broker_pid();
  SANDBOX_ASSERT(kill(broker_pid, SIGKILL) == 0);

  // Now we check that the broker has exited properly.
  int status = 0;
  SANDBOX_ASSERT(waitpid(broker_pid, &status, 0) == broker_pid);
  SANDBOX_ASSERT(WIFSIGNALED(status));
  SANDBOX_ASSERT(WTERMSIG(status) == SIGKILL);
  // Hopefully doing Open with a dead broker won't SIGPIPE us.
  SANDBOX_ASSERT(open_broker.Open("/proc/cpuinfo", O_RDONLY) == -ENOMEM);
}

void TestComplexFlags(bool fast_check_in_client) {
  std::vector<std::string> whitelist;
  whitelist.push_back("/proc/cpuinfo");

  BrokerProcess open_broker(whitelist,
                            whitelist,
                            fast_check_in_client);
  ASSERT_TRUE(open_broker.Init(NULL));
  // Test that we do the right thing for O_CLOEXEC and O_NONBLOCK.
  // Presently, the right thing is to always deny them since they are not
  // supported.
  int fd = -1;
  fd = open_broker.Open("/proc/cpuinfo", O_RDONLY);
  ASSERT_GE(fd, 0);
  ASSERT_EQ(close(fd), 0);

  ASSERT_EQ(open_broker.Open("/proc/cpuinfo", O_RDONLY | O_CLOEXEC), -EPERM);
  ASSERT_EQ(open_broker.Open("/proc/cpuinfo", O_RDONLY | O_NONBLOCK), -EPERM);
}

TEST(BrokerProcess, ComplexFlagsWithClientCheck) {
  TestComplexFlags(true /* fast_check_in_client */);
}

TEST(BrokerProcess, ComplexFlagsNoClientCheck) {
  TestComplexFlags(false /* fast_check_in_client */);
}

}  // namespace sandbox
