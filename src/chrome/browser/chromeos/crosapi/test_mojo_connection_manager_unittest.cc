// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/test_mojo_connection_manager.h"

#include <fcntl.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/process/launch.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/chromeos/crosapi/browser_service_host_ash.h"
#include "chrome/browser/chromeos/crosapi/browser_service_host_observer.h"
#include "chrome/browser/chromeos/crosapi/crosapi_ash.h"
#include "chrome/browser/chromeos/crosapi/crosapi_manager.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/startup/startup_switches.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/fake_user_manager.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/socket_utils_posix.h"
#include "mojo/public/cpp/system/invitation.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace crosapi {
namespace {

class TestBrowserService : public crosapi::mojom::BrowserService {
 public:
  explicit TestBrowserService(
      mojo::PendingReceiver<mojom::BrowserService> receiver)
      : receiver_(this, std::move(receiver)) {}

  ~TestBrowserService() override = default;

  void InitDeprecated(crosapi::mojom::BrowserInitParamsPtr params) override {
    init_is_called_ = true;
  }

  void RequestCrosapiReceiver(
      RequestCrosapiReceiverCallback callback) override {
    EXPECT_TRUE(init_is_called_);
    std::move(callback).Run(crosapi_.BindNewPipeAndPassReceiver());
  }

  void NewWindow(NewWindowCallback callback) override {}
  void GetFeedbackData(GetFeedbackDataCallback callback) override {}
  void GetHistograms(GetHistogramsCallback callback) override {}
  void GetActiveTabUrl(GetActiveTabUrlCallback callback) override {}

  bool init_is_called() { return init_is_called_; }

 private:
  mojo::Receiver<mojom::BrowserService> receiver_;

  bool init_is_called_ = false;

  mojo::Remote<crosapi::mojom::Crosapi> crosapi_;
};

class TestBrowserServiceHostObserver : public BrowserServiceHostObserver {
 public:
  // |callback| is invoked when OnBrowserServiceConnected is called
  // |num_calls| times.
  TestBrowserServiceHostObserver(size_t num_calls, base::OnceClosure callback)
      : remaining_num_calls_(num_calls), callback_(std::move(callback)) {
    DCHECK_GT(num_calls, 0);
  }

  void OnBrowserServiceConnected(CrosapiId id,
                                 mojo::RemoteSetElementId mojo_id,
                                 mojom::BrowserService* browser_service,
                                 uint32_t browser_service_version) override {
    --remaining_num_calls_;
    if (remaining_num_calls_ == 0)
      std::move(callback_).Run();
  }

 private:
  size_t remaining_num_calls_;
  base::OnceClosure callback_;
};

std::vector<base::ScopedFD> ConnectTestingMojoSocket(
    const std::string& socket_path) {
  auto channel = mojo::NamedPlatformChannel::ConnectToServer(socket_path);
  if (!channel.is_valid())
    return {};
  base::ScopedFD socket_fd = channel.TakePlatformHandle().TakeFD();

  uint8_t buf[32];
  std::vector<base::ScopedFD> descriptors;
  ssize_t size;
  base::RunLoop run_loop;
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(base::BindLambdaForTesting([&]() {
        // Mark the channel as blocking.
        int flags = fcntl(socket_fd.get(), F_GETFL);
        PCHECK(flags != -1);
        fcntl(socket_fd.get(), F_SETFL, flags & ~O_NONBLOCK);
        size = mojo::SocketRecvmsg(socket_fd.get(), buf, sizeof(buf),
                                   &descriptors, true /*block*/);
      })),
      run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(1, size);
  EXPECT_EQ(0u, buf[0]);
  return descriptors;
}

base::Process LaunchTestSubprocess(std::vector<base::ScopedFD> descriptors) {
  base::LaunchOptions options;
  options.fds_to_remap.emplace_back(descriptors[0].get(), descriptors[0].get());
  base::CommandLine cmd(base::GetMultiProcessTestChildBaseCommandLine());
  cmd.AppendSwitchASCII(mojo::PlatformChannel::kHandleSwitch,
                        base::NumberToString(descriptors[0].get()));
  cmd.AppendSwitchASCII(chromeos::switches::kCrosStartupDataFD,
                        base::NumberToString(descriptors[1].get()));
  return base::SpawnMultiProcessTestChild("CrosapiClientMain", cmd, options);
}

}  // namespace

using TestMojoConnectionManagerTest = testing::Test;

TEST_F(TestMojoConnectionManagerTest, ConnectMultipleClients) {
  // Create temp dir before task environment, just in case lingering tasks need
  // to access it.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Use IO type to support the FileDescriptorWatcher API on POSIX.
  // TestingProfileManager instantiated below requires a TaskRunner.
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::MainThreadType::IO};

  chromeos::LoginState::Initialize();
  base::ScopedClosureRunner login_state_teardown(
      base::BindOnce(&chromeos::LoginState::Shutdown));

  // Constructing CrosapiManager requires ProfileManager.
  // Also, constructing BrowserInitParams requires local state prefs.
  TestingProfileManager testing_profile_manager(
      TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(testing_profile_manager.SetUp());

  // Set up UserManager to fake the login state.
  user_manager::FakeUserManager user_manager;
  user_manager.Initialize();
  base::ScopedClosureRunner user_manager_teardown(
      base::BindOnce(base::BindLambdaForTesting(
          [&user_manager]() { user_manager.Destroy(); })));
  const AccountId account = AccountId::FromUserEmail("test@test");
  const user_manager::User* user = user_manager.AddUser(account);
  user_manager.UserLoggedIn(account, user->username_hash(), false, false);

  auto crosapi_manager = std::make_unique<CrosapiManager>();

  // Ash-chrome queues an invitation, drop a socket and wait for connection.
  std::string socket_path =
      temp_dir.GetPath().MaybeAsASCII() + "/lacros.socket";

  // Sets up watcher to Wait for the socket to be created by
  // |TestMojoConnectionManager| before attempting to connect. There is no
  // garanteen that |OnTestingSocketCreated| has run after the run loop is done,
  // so this test should NOT depend on the assumption.
  base::FilePathWatcher watcher;
  base::RunLoop run_loop1;
  watcher.Watch(base::FilePath(socket_path),
                base::FilePathWatcher::Type::kNonRecursive,
                base::BindRepeating(base::BindLambdaForTesting(
                    [&run_loop1](const base::FilePath& path, bool error) {
                      EXPECT_FALSE(error);
                      run_loop1.Quit();
                    })));
  TestMojoConnectionManager test_mojo_connection_manager{
      base::FilePath(socket_path)};
  run_loop1.Run();

  // Test connects with ash-chrome via the socket.
  std::vector<base::ScopedFD> descriptors1 =
      ConnectTestingMojoSocket(socket_path);
  ASSERT_EQ(3u, descriptors1.size());
  std::vector<base::ScopedFD> descriptors2 =
      ConnectTestingMojoSocket(socket_path);
  ASSERT_EQ(3u, descriptors2.size());

  base::RunLoop run_loop2;
  // Two BrowserService connections should be made (one for each subprocess).
  TestBrowserServiceHostObserver observer(2, run_loop2.QuitClosure());
  crosapi_manager->crosapi_ash()->browser_service_host_ash()->AddObserver(
      &observer);
  base::ScopedClosureRunner observer_reset(base::BindOnce(
      base::BindLambdaForTesting([&observer, &crosapi_manager]() {
        crosapi_manager->crosapi_ash()
            ->browser_service_host_ash()
            ->RemoveObserver(&observer);
      })));

  // Then launch two subprocesses running in parallel.
  // These will connect BrowserService mojo.
  base::Process sub1 = LaunchTestSubprocess(std::move(descriptors1));
  base::Process sub2 = LaunchTestSubprocess(std::move(descriptors2));

  // Wait for two BrowserService instances.
  run_loop2.Run();

  // Connection succeeded. Terminate subprocesses.
  ASSERT_TRUE(base::TerminateMultiProcessTestChild(sub1, 0, true));
  sub1.Close();
  ASSERT_TRUE(base::TerminateMultiProcessTestChild(sub2, 0, true));
  sub2.Close();
}

// Another process that emulates the behavior of lacros-chrome.
MULTIPROCESS_TEST_MAIN(CrosapiClientMain) {
  base::test::SingleThreadTaskEnvironment task_environment;
  mojo::IncomingInvitation invitation = mojo::IncomingInvitation::Accept(
      mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
          *base::CommandLine::ForCurrentProcess()));
  base::RunLoop run_loop;
  TestBrowserService test_browser_service(
      mojo::PendingReceiver<crosapi::mojom::BrowserService>(
          invitation.ExtractMessagePipe(0)));
  // Do not return from the run loop.
  run_loop.Run();
  return 0;
}

}  // namespace crosapi
