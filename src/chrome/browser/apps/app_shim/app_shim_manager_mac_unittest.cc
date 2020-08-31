// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_shim/app_shim_manager_mac.h"

#include <unistd.h>

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/apps/app_shim/app_shim_host_bootstrap_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_host_mac.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/web_applications/components/app_shim_registry_mac.h"
#include "chrome/common/mac/app_shim.mojom.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::WithArgs;

class MockDelegate : public AppShimManager::Delegate {
 public:
  virtual ~MockDelegate() {}

  MOCK_METHOD2(ShowAppWindows, bool(Profile*, const std::string&));
  MOCK_METHOD2(CloseAppWindows, void(Profile*, const std::string&));

  MOCK_METHOD2(AppIsInstalled, bool(Profile*, const std::string&));
  MOCK_METHOD2(AppUsesRemoteCocoa, bool(Profile*, const std::string&));
  MOCK_METHOD2(AppIsMultiProfile, bool(Profile*, const std::string&));
  MOCK_METHOD3(EnableExtension,
               void(Profile*, const std::string&, base::OnceCallback<void()>));
  MOCK_METHOD3(LaunchApp,
               void(Profile*,
                    const std::string& app_id,
                    const std::vector<base::FilePath>&));

  // Conditionally mock LaunchShim. Some tests will execute |launch_callback|
  // with a particular value.
  MOCK_METHOD3(DoLaunchShim, void(Profile*, const std::string&, bool));
  void LaunchShim(Profile* profile,
                  const std::string& app_id,
                  bool recreate_shim,
                  ShimLaunchedCallback launched_callback,
                  ShimTerminatedCallback terminated_callback) override {
    if (launch_shim_callback_capture_)
      *launch_shim_callback_capture_ = std::move(launched_callback);
    if (terminated_shim_callback_capture_)
      *terminated_shim_callback_capture_ = std::move(terminated_callback);
    DoLaunchShim(profile, app_id, recreate_shim);
  }
  void SetCaptureShimLaunchedCallback(ShimLaunchedCallback* callback) {
    launch_shim_callback_capture_ = callback;
  }
  void SetCaptureShimTerminatedCallback(ShimTerminatedCallback* callback) {
    terminated_shim_callback_capture_ = callback;
  }

  MOCK_METHOD0(HasNonBookmarkAppWindowsOpen, bool());

  void SetAppCanCreateHost(bool should_create_host) {
    allow_shim_to_connect_ = should_create_host;
  }
  bool AppCanCreateHost(Profile* profile, const std::string& app_id) override {
    return allow_shim_to_connect_;
  }

 private:
  ShimLaunchedCallback* launch_shim_callback_capture_ = nullptr;
  ShimTerminatedCallback* terminated_shim_callback_capture_ = nullptr;
  bool allow_shim_to_connect_ = true;
};

class TestingAppShimManager : public AppShimManager {
 public:
  TestingAppShimManager(std::unique_ptr<Delegate> delegate)
      : AppShimManager(std::move(delegate)) {}
  virtual ~TestingAppShimManager() { DCHECK(load_profile_callbacks_.empty()); }

  MOCK_METHOD1(OnShimFocus, void(AppShimHost* host));

  void RealOnShimFocus(AppShimHost* host) { AppShimManager::OnShimFocus(host); }

  void SetProfileMenuItems(
      std::vector<chrome::mojom::ProfileMenuItemPtr> new_profile_menu_items) {
    new_profile_menu_items_ = std::move(new_profile_menu_items);
    OnAvatarMenuChanged(nullptr);
  }
  void RebuildProfileMenuItemsFromAvatarMenu() override {
    profile_menu_items_.clear();
    for (const auto& item : new_profile_menu_items_)
      profile_menu_items_.push_back(item.Clone());
  }

  void SetAcceptablyCodeSigned(bool is_acceptable_code_signed) {
    is_acceptably_code_signed_ = is_acceptable_code_signed;
  }
  bool IsAcceptablyCodeSigned(pid_t pid) const override {
    return is_acceptably_code_signed_;
  }

  MOCK_METHOD1(ProfileForPath, Profile*(const base::FilePath&));
  void LoadProfileAsync(const base::FilePath& path,
                        base::OnceCallback<void(Profile*)> callback) override {
    CaptureLoadProfileCallback(path, std::move(callback));
  }
  MOCK_METHOD1(IsProfileLockedForPath, bool(const base::FilePath&));
  void SetHostForCreate(std::unique_ptr<AppShimHost> host_for_create) {
    host_for_create_ = std::move(host_for_create);
  }
  std::unique_ptr<AppShimHost> CreateHost(AppShimHost::Client* client,
                                          const base::FilePath& profile_path,
                                          const std::string& app_id,
                                          bool use_remote_cocoa) override {
    DCHECK(host_for_create_);
    std::unique_ptr<AppShimHost> result = std::move(host_for_create_);
    return result;
  }
  MOCK_METHOD2(OpenAppURLInBrowserWindow,
               void(const base::FilePath&, const GURL& url));
  MOCK_METHOD0(LaunchUserManager, void());
  MOCK_METHOD0(MaybeTerminate, void());

  void CaptureLoadProfileCallback(const base::FilePath& path,
                                  base::OnceCallback<void(Profile*)> callback) {
    load_profile_callbacks_[path] = std::move(callback);
  }
  bool RunLoadProfileCallback(const base::FilePath& path, Profile* profile) {
    std::move(load_profile_callbacks_[path]).Run(profile);
    return load_profile_callbacks_.erase(path);
  }

  content::NotificationRegistrar& GetRegistrar() { return registrar(); }

 private:
  std::map<base::FilePath, base::OnceCallback<void(Profile*)>>
      load_profile_callbacks_;
  std::unique_ptr<AppShimHost> host_for_create_ = nullptr;
  std::vector<chrome::mojom::ProfileMenuItemPtr> new_profile_menu_items_;
  bool is_acceptably_code_signed_ = true;
  DISALLOW_COPY_AND_ASSIGN(TestingAppShimManager);
};

class TestingAppShimHostBootstrap : public AppShimHostBootstrap {
 public:
  TestingAppShimHostBootstrap(
      const base::FilePath& profile_path,
      const std::string& app_id,
      bool is_from_bookmark,
      base::Optional<chrome::mojom::AppShimLaunchResult>* launch_result)
      : AppShimHostBootstrap(getpid()),
        profile_path_(profile_path),
        app_id_(app_id),
        is_from_bookmark_(is_from_bookmark),
        launch_result_(launch_result),
        weak_factory_(this) {}
  void DoTestLaunch(chrome::mojom::AppShimLaunchType launch_type,
                    const std::vector<base::FilePath>& files) {
    mojo::Remote<chrome::mojom::AppShimHost> host;
    auto app_shim_info = chrome::mojom::AppShimInfo::New();
    app_shim_info->profile_path = profile_path_;
    app_shim_info->app_id = app_id_;
    if (is_from_bookmark_)
      app_shim_info->app_url = GURL("https://example.com");
    app_shim_info->launch_type = launch_type;
    app_shim_info->files = files;
    OnShimConnected(
        host.BindNewPipeAndPassReceiver(), std::move(app_shim_info),
        base::BindOnce(&TestingAppShimHostBootstrap::DoTestLaunchDone,
                       launch_result_));
  }

  static void DoTestLaunchDone(
      base::Optional<chrome::mojom::AppShimLaunchResult>* launch_result,
      chrome::mojom::AppShimLaunchResult result,
      mojo::PendingReceiver<chrome::mojom::AppShim> app_shim_receiver) {
    if (launch_result)
      launch_result->emplace(result);
  }

  base::WeakPtr<TestingAppShimHostBootstrap> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  const base::FilePath profile_path_;
  const std::string app_id_;
  const bool is_from_bookmark_;
  // Note that |launch_result_| is optional so that we can track whether or not
  // the callback to set it has arrived.
  base::Optional<chrome::mojom::AppShimLaunchResult>* launch_result_;
  base::WeakPtrFactory<TestingAppShimHostBootstrap> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(TestingAppShimHostBootstrap);
};

const char kTestAppIdA[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kTestAppIdB[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

class TestAppShim : public chrome::mojom::AppShim {
 public:
  // chrome::mojom::AppShim:
  void CreateRemoteCocoaApplication(
      mojo::PendingAssociatedReceiver<remote_cocoa::mojom::Application>
          receiver) override {}
  void CreateCommandDispatcherForWidget(uint64_t widget_id) override {}
  void SetBadgeLabel(const std::string& badge_label) override {}
  void SetUserAttention(
      chrome::mojom::AppShimAttentionType attention_type) override {}
  void UpdateProfileMenu(std::vector<chrome::mojom::ProfileMenuItemPtr>
                             profile_menu_items) override {
    profile_menu_items_ = std::move(profile_menu_items);
  }

  std::vector<chrome::mojom::ProfileMenuItemPtr> profile_menu_items_;
};

class TestHost : public AppShimHost {
 public:
  TestHost(const base::FilePath& profile_path,
           const std::string& app_id,
           TestingAppShimManager* manager)
      : AppShimHost(manager,
                    app_id,
                    profile_path,
                    false /* uses_remote_views */),
        test_app_shim_(new TestAppShim),
        test_weak_factory_(this) {}
  ~TestHost() override {}

  chrome::mojom::AppShim* GetAppShim() const override {
    return test_app_shim_.get();
  }

  // Record whether or not OnBootstrapConnected has been called.
  void OnBootstrapConnected(
      std::unique_ptr<AppShimHostBootstrap> bootstrap) override {
    EXPECT_FALSE(did_connect_to_host_);
    did_connect_to_host_ = true;
    AppShimHost::OnBootstrapConnected(std::move(bootstrap));
  }
  bool did_connect_to_host() const { return did_connect_to_host_; }

  base::WeakPtr<TestHost> GetWeakPtr() {
    return test_weak_factory_.GetWeakPtr();
  }

  using AppShimHost::FilesOpened;
  using AppShimHost::ProfileSelectedFromMenu;

  std::unique_ptr<TestAppShim> test_app_shim_;

 private:
  bool did_connect_to_host_ = false;

  base::WeakPtrFactory<TestHost> test_weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(TestHost);
};

class AppShimManagerTest : public testing::Test {
 protected:
  AppShimManagerTest() {}
  ~AppShimManagerTest() override {}

  void SetUp() override {
    profile_path_a_ = profile_a_.GetPath();
    profile_path_b_ = profile_b_.GetPath();
    profile_path_c_ = profile_c_.GetPath();
    const base::FilePath user_data_dir = profile_path_a_.DirName();

    local_state_ = std::make_unique<TestingPrefServiceSimple>();
    AppShimRegistry::Get()->RegisterLocalPrefs(local_state_->registry());
    AppShimRegistry::Get()->SetPrefServiceAndUserDataDirForTesting(
        local_state_.get(), user_data_dir);

    std::unique_ptr<MockDelegate> delegate = std::make_unique<MockDelegate>();
    delegate_ = delegate.get();
    manager_.reset(new TestingAppShimManager(std::move(delegate)));
    AppShimHostBootstrap::SetClient(manager_.get());
    bootstrap_aa_ = (new TestingAppShimHostBootstrap(
                         profile_path_a_, kTestAppIdA,
                         true /* is_from_bookmark */, &bootstrap_aa_result_))
                        ->GetWeakPtr();
    bootstrap_ba_ = (new TestingAppShimHostBootstrap(
                         profile_path_b_, kTestAppIdA,
                         true /* is_from_bookmark */, &bootstrap_ba_result_))
                        ->GetWeakPtr();
    bootstrap_ca_ = (new TestingAppShimHostBootstrap(
                         profile_path_c_, kTestAppIdA,
                         true /* is_from_bookmark */, &bootstrap_ca_result_))
                        ->GetWeakPtr();
    bootstrap_xa_ = (new TestingAppShimHostBootstrap(
                         base::FilePath(), kTestAppIdA,
                         true /* is_from_bookmark */, &bootstrap_xa_result_))
                        ->GetWeakPtr();
    bootstrap_ab_ = (new TestingAppShimHostBootstrap(
                         profile_path_a_, kTestAppIdB,
                         false /* is_from_bookmark */, &bootstrap_ab_result_))
                        ->GetWeakPtr();
    bootstrap_bb_ = (new TestingAppShimHostBootstrap(
                         profile_path_b_, kTestAppIdB,
                         false /* is_from_bookmark */, &bootstrap_bb_result_))
                        ->GetWeakPtr();
    bootstrap_aa_duplicate_ =
        (new TestingAppShimHostBootstrap(profile_path_a_, kTestAppIdA,
                                         true /* is_from_bookmark */,
                                         &bootstrap_aa_duplicate_result_))
            ->GetWeakPtr();
    bootstrap_aa_thethird_ =
        (new TestingAppShimHostBootstrap(profile_path_a_, kTestAppIdA,
                                         true /* is_from_bookmark */,
                                         &bootstrap_aa_thethird_result_))
            ->GetWeakPtr();

    host_aa_unique_ = std::make_unique<TestHost>(profile_path_a_, kTestAppIdA,
                                                 manager_.get());
    host_ab_unique_ = std::make_unique<TestHost>(profile_path_a_, kTestAppIdB,
                                                 manager_.get());
    host_ba_unique_ = std::make_unique<TestHost>(profile_path_b_, kTestAppIdA,
                                                 manager_.get());
    host_bb_unique_ = std::make_unique<TestHost>(profile_path_b_, kTestAppIdB,
                                                 manager_.get());
    host_aa_duplicate_unique_ = std::make_unique<TestHost>(
        profile_path_a_, kTestAppIdA, manager_.get());

    host_aa_ = host_aa_unique_->GetWeakPtr();
    host_ab_ = host_ab_unique_->GetWeakPtr();
    host_ba_ = host_ba_unique_->GetWeakPtr();
    host_bb_ = host_bb_unique_->GetWeakPtr();

    base::FilePath extension_path("/fake/path");

    EXPECT_CALL(*delegate_, AppIsMultiProfile(_, kTestAppIdA))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*delegate_, AppIsMultiProfile(_, kTestAppIdB))
        .WillRepeatedly(Return(false));

    EXPECT_CALL(*delegate_, AppUsesRemoteCocoa(_, _))
        .WillRepeatedly(Return(true));

    {
      auto item_a = chrome::mojom::ProfileMenuItem::New();
      item_a->profile_path = profile_path_a_;
      item_a->menu_index = 0;
      auto item_b = chrome::mojom::ProfileMenuItem::New();
      item_b->profile_path = profile_path_b_;
      item_b->menu_index = 1;
      std::vector<chrome::mojom::ProfileMenuItemPtr> items;
      items.push_back(std::move(item_a));
      items.push_back(std::move(item_b));
      manager_->SetProfileMenuItems(std::move(items));
    }

    // Tests that expect this call will override it.
    EXPECT_CALL(*manager_, OpenAppURLInBrowserWindow(_, _)).Times(0);

    EXPECT_CALL(*manager_, IsProfileLockedForPath(profile_path_a_))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*manager_, ProfileForPath(profile_path_a_))
        .WillRepeatedly(Return(&profile_a_));

    EXPECT_CALL(*manager_, IsProfileLockedForPath(profile_path_b_))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*manager_, ProfileForPath(profile_path_b_))
        .WillRepeatedly(Return(&profile_b_));

    EXPECT_CALL(*manager_, IsProfileLockedForPath(profile_path_c_))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*manager_, ProfileForPath(profile_path_c_))
        .WillRepeatedly(Return(&profile_c_));

    EXPECT_CALL(*delegate_, AppIsInstalled(&profile_a_, kTestAppIdA))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*delegate_, AppIsInstalled(&profile_b_, kTestAppIdA))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*delegate_, AppIsInstalled(&profile_c_, kTestAppIdA))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*delegate_, AppIsInstalled(_, kTestAppIdB))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*delegate_, LaunchApp(_, _, _)).WillRepeatedly(Return());
  }

  void TearDown() override {
    host_aa_unique_.reset();
    host_ab_unique_.reset();
    host_bb_unique_.reset();
    host_aa_duplicate_unique_.reset();
    manager_->SetHostForCreate(nullptr);
    manager_.reset();

    // Delete the bootstraps via their weak pointers if they haven't been
    // deleted yet. Note that this must be done after the profiles and hosts
    // have been destroyed (because they may now own the bootstraps).
    delete bootstrap_aa_.get();
    delete bootstrap_ba_.get();
    delete bootstrap_ca_.get();
    delete bootstrap_xa_.get();
    delete bootstrap_ab_.get();
    delete bootstrap_bb_.get();
    delete bootstrap_aa_duplicate_.get();
    delete bootstrap_aa_thethird_.get();

    AppShimHostBootstrap::SetClient(nullptr);

    AppShimRegistry::Get()->SetPrefServiceAndUserDataDirForTesting(
        nullptr, base::FilePath());
  }

  void DoShimLaunch(base::WeakPtr<TestingAppShimHostBootstrap> bootstrap,
                    std::unique_ptr<TestHost> host,
                    chrome::mojom::AppShimLaunchType launch_type,
                    const std::vector<base::FilePath>& files) {
    if (host)
      manager_->SetHostForCreate(std::move(host));
    bootstrap->DoTestLaunch(launch_type, files);
  }

  void NormalLaunch(base::WeakPtr<TestingAppShimHostBootstrap> bootstrap,
                    std::unique_ptr<TestHost> host) {
    DoShimLaunch(bootstrap, std::move(host),
                 chrome::mojom::AppShimLaunchType::kNormal,
                 std::vector<base::FilePath>());
  }

  void RegisterOnlyLaunch(base::WeakPtr<TestingAppShimHostBootstrap> bootstrap,
                          std::unique_ptr<TestHost> host) {
    DoShimLaunch(bootstrap, std::move(host),
                 chrome::mojom::AppShimLaunchType::kRegisterOnly,
                 std::vector<base::FilePath>());
  }

  // Completely launch a shim host and leave it running.
  void LaunchAndActivate(base::WeakPtr<TestingAppShimHostBootstrap> bootstrap,
                         std::unique_ptr<TestHost> host_unique,
                         Profile* profile) {
    base::WeakPtr<TestHost> host = host_unique->GetWeakPtr();
    NormalLaunch(bootstrap, std::move(host_unique));
    EXPECT_EQ(host.get(), manager_->FindHost(profile, host->GetAppId()));
    EXPECT_CALL(*manager_, OnShimFocus(host.get()));
    manager_->OnAppActivated(profile, host->GetAppId());
    EXPECT_TRUE(host->did_connect_to_host());
  }

  // Simulates a focus request coming from a running app shim.
  void ShimNormalFocus(TestHost* host) {
    EXPECT_CALL(*manager_, OnShimFocus(host))
        .WillOnce(
            Invoke(manager_.get(), &TestingAppShimManager::RealOnShimFocus));
    manager_->OnShimFocus(host);
  }

  content::BrowserTaskEnvironment task_environment_;
  MockDelegate* delegate_;
  std::unique_ptr<TestingAppShimManager> manager_;
  base::FilePath profile_path_a_;
  base::FilePath profile_path_b_;
  base::FilePath profile_path_c_;
  TestingProfile profile_a_;
  TestingProfile profile_b_;
  TestingProfile profile_c_;

  base::WeakPtr<TestingAppShimHostBootstrap> bootstrap_aa_;
  base::WeakPtr<TestingAppShimHostBootstrap> bootstrap_ba_;
  base::WeakPtr<TestingAppShimHostBootstrap> bootstrap_ca_;
  base::WeakPtr<TestingAppShimHostBootstrap> bootstrap_xa_;
  base::WeakPtr<TestingAppShimHostBootstrap> bootstrap_ab_;
  base::WeakPtr<TestingAppShimHostBootstrap> bootstrap_bb_;
  base::WeakPtr<TestingAppShimHostBootstrap> bootstrap_aa_duplicate_;
  base::WeakPtr<TestingAppShimHostBootstrap> bootstrap_aa_thethird_;

  base::Optional<chrome::mojom::AppShimLaunchResult> bootstrap_aa_result_;
  base::Optional<chrome::mojom::AppShimLaunchResult> bootstrap_ba_result_;
  base::Optional<chrome::mojom::AppShimLaunchResult> bootstrap_ca_result_;
  base::Optional<chrome::mojom::AppShimLaunchResult> bootstrap_xa_result_;
  base::Optional<chrome::mojom::AppShimLaunchResult> bootstrap_ab_result_;
  base::Optional<chrome::mojom::AppShimLaunchResult> bootstrap_bb_result_;
  base::Optional<chrome::mojom::AppShimLaunchResult>
      bootstrap_aa_duplicate_result_;
  base::Optional<chrome::mojom::AppShimLaunchResult>
      bootstrap_aa_thethird_result_;

  // Unique ptr to the TestsHosts used by the tests. These are passed by
  // std::move durnig tests. To access them after they have been passed, use
  // the WeakPtr versions.
  std::unique_ptr<TestHost> host_aa_unique_;
  std::unique_ptr<TestHost> host_ab_unique_;
  std::unique_ptr<TestHost> host_ba_unique_;
  std::unique_ptr<TestHost> host_bb_unique_;
  std::unique_ptr<TestHost> host_aa_duplicate_unique_;

  base::WeakPtr<TestHost> host_aa_;
  base::WeakPtr<TestHost> host_ab_;
  base::WeakPtr<TestHost> host_ba_;
  base::WeakPtr<TestHost> host_bb_;

 private:
  std::unique_ptr<TestingPrefServiceSimple> local_state_;
  DISALLOW_COPY_AND_ASSIGN(AppShimManagerTest);
};

TEST_F(AppShimManagerTest, LaunchProfileNotFound) {
  // Bad profile path, opens a bookmark app in a new window.
  EXPECT_CALL(*manager_, ProfileForPath(profile_path_a_))
      .WillRepeatedly(Return(static_cast<Profile*>(nullptr)));
  NormalLaunch(bootstrap_aa_, nullptr);
  EXPECT_CALL(*manager_, OpenAppURLInBrowserWindow(profile_path_a_, _));
  manager_->RunLoadProfileCallback(profile_path_a_, nullptr);
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kProfileNotFound,
            *bootstrap_aa_result_);
}

TEST_F(AppShimManagerTest, LaunchProfileNotFoundNotBookmark) {
  // Bad profile path, not a bookmark app, doesn't open anything.
  EXPECT_CALL(*manager_, ProfileForPath(profile_path_a_))
      .WillRepeatedly(Return(static_cast<Profile*>(nullptr)));
  NormalLaunch(bootstrap_ab_, nullptr);
  manager_->RunLoadProfileCallback(profile_path_a_, nullptr);
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kProfileNotFound,
            *bootstrap_ab_result_);
}

TEST_F(AppShimManagerTest, LaunchProfileIsLocked) {
  // Profile is locked.
  EXPECT_CALL(*manager_, IsProfileLockedForPath(profile_path_a_))
      .WillOnce(Return(true));
  EXPECT_CALL(*manager_, LaunchUserManager());
  EXPECT_CALL(*manager_, OpenAppURLInBrowserWindow(profile_path_a_, _));
  NormalLaunch(bootstrap_aa_, nullptr);
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kProfileLocked,
            *bootstrap_aa_result_);
}

TEST_F(AppShimManagerTest, LaunchAppNotFound) {
  // App not found.
  EXPECT_CALL(*delegate_, AppIsInstalled(&profile_a_, kTestAppIdA))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*delegate_, EnableExtension(&profile_a_, kTestAppIdA, _))
      .WillOnce(RunOnceCallback<2>());
  EXPECT_CALL(*manager_, OpenAppURLInBrowserWindow(profile_path_a_, _));
  NormalLaunch(bootstrap_aa_, std::move(host_aa_unique_));
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kAppNotFound,
            *bootstrap_aa_result_);
}

TEST_F(AppShimManagerTest, LaunchAppNotEnabled) {
  // App not found.
  EXPECT_CALL(*delegate_, AppIsInstalled(&profile_a_, kTestAppIdA))
      .WillOnce(Return(false))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*delegate_, EnableExtension(&profile_a_, kTestAppIdA, _))
      .WillOnce(RunOnceCallback<2>());
  NormalLaunch(bootstrap_aa_, std::move(host_aa_unique_));
}

TEST_F(AppShimManagerTest, LaunchAndCloseShim) {
  // Normal startup.
  NormalLaunch(bootstrap_aa_, std::move(host_aa_unique_));
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));

  NormalLaunch(bootstrap_ab_, std::move(host_ab_unique_));
  EXPECT_EQ(host_ab_.get(), manager_->FindHost(&profile_a_, kTestAppIdB));

  std::vector<base::FilePath> some_file(1, base::FilePath("some_file"));
  EXPECT_CALL(*delegate_, LaunchApp(&profile_b_, kTestAppIdB, some_file));
  DoShimLaunch(bootstrap_bb_, std::move(host_bb_unique_),
               chrome::mojom::AppShimLaunchType::kNormal, some_file);
  EXPECT_EQ(host_bb_.get(), manager_->FindHost(&profile_b_, kTestAppIdB));

  // Activation when there is a registered shim finishes launch with success and
  // focuses the app.
  EXPECT_CALL(*manager_, OnShimFocus(host_aa_.get()));
  manager_->OnAppActivated(&profile_a_, kTestAppIdA);
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kSuccess,
            *bootstrap_aa_result_);

  // Starting and closing a second host just focuses the original host of the
  // app.
  EXPECT_CALL(*manager_, OnShimFocus(host_aa_.get()));

  DoShimLaunch(bootstrap_aa_duplicate_, std::move(host_aa_duplicate_unique_),
               chrome::mojom::AppShimLaunchType::kNormal, some_file);
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kDuplicateHost,
            *bootstrap_aa_duplicate_result_);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));

  // Normal close.
  manager_->OnShimProcessDisconnected(host_aa_.get());
  EXPECT_FALSE(manager_->FindHost(&profile_a_, kTestAppIdA));
  EXPECT_EQ(host_aa_.get(), nullptr);
}

TEST_F(AppShimManagerTest, AppLifetime) {
  // When the app activates, a host is created. If there is no shim, one is
  // launched.
  manager_->SetHostForCreate(std::move(host_aa_unique_));
  EXPECT_CALL(*delegate_, DoLaunchShim(&profile_a_, kTestAppIdA, false));
  manager_->OnAppActivated(&profile_a_, kTestAppIdA);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));

  // Normal shim launch adds an entry in the map.
  // App should not be launched here, but return success to the shim.
  EXPECT_CALL(*delegate_, LaunchApp(&profile_a_, kTestAppIdA, _)).Times(0);
  RegisterOnlyLaunch(bootstrap_aa_, nullptr);
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kSuccess,
            *bootstrap_aa_result_);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));

  // Return no app windows for OnShimFocus. This will result in a launch call.
  EXPECT_CALL(*delegate_, ShowAppWindows(&profile_a_, kTestAppIdA))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*delegate_, LaunchApp(&profile_a_, kTestAppIdA, _)).Times(1);
  ShimNormalFocus(host_aa_.get());

  // Return one window. This should do nothing.
  EXPECT_CALL(*delegate_, ShowAppWindows(&profile_a_, kTestAppIdA))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*delegate_, LaunchApp(&profile_a_, kTestAppIdA, _)).Times(0);
  ShimNormalFocus(host_aa_.get());

  // Open files should trigger a launch with those files.
  std::vector<base::FilePath> some_file(1, base::FilePath("some_file"));
  EXPECT_CALL(*delegate_, LaunchApp(&profile_a_, kTestAppIdA, some_file));
  host_aa_->FilesOpened(some_file);

  // Process disconnect will cause the host to be deleted.
  manager_->OnShimProcessDisconnected(host_aa_.get());
  EXPECT_EQ(nullptr, host_aa_.get());

  // OnAppDeactivated should trigger a MaybeTerminate call.
  EXPECT_CALL(*manager_, MaybeTerminate()).WillOnce(Return());
  manager_->OnAppDeactivated(&profile_a_, kTestAppIdA);
}

TEST_F(AppShimManagerTest, FailToLaunch) {
  // When the app activates, it requests a launch.
  ShimLaunchedCallback launch_callback;
  delegate_->SetCaptureShimLaunchedCallback(&launch_callback);
  manager_->SetHostForCreate(std::move(host_aa_unique_));
  EXPECT_CALL(*delegate_, DoLaunchShim(&profile_a_, kTestAppIdA, false));
  manager_->OnAppActivated(&profile_a_, kTestAppIdA);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));
  EXPECT_TRUE(launch_callback);

  // Run the callback claiming that the launch failed. This should trigger
  // another launch, this time forcing shim recreation.
  EXPECT_CALL(*delegate_, DoLaunchShim(&profile_a_, kTestAppIdA, true));
  std::move(launch_callback).Run(base::Process());
  EXPECT_TRUE(launch_callback);

  // Report that the launch failed. This should trigger deletion of the host.
  EXPECT_NE(nullptr, host_aa_.get());
  std::move(launch_callback).Run(base::Process());
  EXPECT_EQ(nullptr, host_aa_.get());
}

TEST_F(AppShimManagerTest, FailToConnect) {
  // When the app activates, it requests a launch.
  ShimLaunchedCallback launched_callback;
  delegate_->SetCaptureShimLaunchedCallback(&launched_callback);
  ShimTerminatedCallback terminated_callback;
  delegate_->SetCaptureShimTerminatedCallback(&terminated_callback);

  manager_->SetHostForCreate(std::move(host_aa_unique_));
  EXPECT_CALL(*delegate_, DoLaunchShim(&profile_a_, kTestAppIdA, false));
  manager_->OnAppActivated(&profile_a_, kTestAppIdA);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));
  EXPECT_TRUE(launched_callback);
  EXPECT_TRUE(terminated_callback);

  // Run the launch callback claiming that the launch succeeded.
  std::move(launched_callback).Run(base::Process(5));
  EXPECT_FALSE(launched_callback);
  EXPECT_TRUE(terminated_callback);

  // Report that the process terminated. This should trigger a re-create and
  // re-launch.
  EXPECT_CALL(*delegate_, DoLaunchShim(&profile_a_, kTestAppIdA, true));
  std::move(terminated_callback).Run();
  EXPECT_TRUE(launched_callback);
  EXPECT_TRUE(terminated_callback);

  // Run the launch callback claiming that the launch succeeded.
  std::move(launched_callback).Run(base::Process(7));
  EXPECT_FALSE(launched_callback);
  EXPECT_TRUE(terminated_callback);

  // Report that the process terminated again. This should trigger deletion of
  // the host.
  EXPECT_NE(nullptr, host_aa_.get());
  std::move(terminated_callback).Run();
  EXPECT_EQ(nullptr, host_aa_.get());
}

TEST_F(AppShimManagerTest, FailCodeSignature) {
  manager_->SetAcceptablyCodeSigned(false);
  ShimLaunchedCallback launched_callback;
  delegate_->SetCaptureShimLaunchedCallback(&launched_callback);
  ShimTerminatedCallback terminated_callback;
  delegate_->SetCaptureShimTerminatedCallback(&terminated_callback);

  // Fail to code-sign. This should result in a host being created, and a launch
  // having been requested.
  EXPECT_CALL(*delegate_, DoLaunchShim(&profile_a_, kTestAppIdA, false));
  NormalLaunch(bootstrap_aa_, std::move(host_aa_unique_));
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));
  EXPECT_TRUE(launched_callback);
  EXPECT_TRUE(terminated_callback);
  EXPECT_FALSE(host_aa_->HasBootstrapConnected());

  // Run the launch callback claiming that the launch succeeded.
  std::move(launched_callback).Run(base::Process(5));
  EXPECT_FALSE(launched_callback);
  EXPECT_TRUE(terminated_callback);
  EXPECT_FALSE(host_aa_->HasBootstrapConnected());

  // Simulate the register call that then fails due to signature failing.
  RegisterOnlyLaunch(bootstrap_aa_duplicate_, std::move(host_aa_unique_));
  EXPECT_FALSE(host_aa_->HasBootstrapConnected());

  // Simulate the termination after the register failed.
  manager_->SetAcceptablyCodeSigned(true);
  EXPECT_CALL(*delegate_, DoLaunchShim(&profile_a_, kTestAppIdA, true));
  std::move(terminated_callback).Run();
  EXPECT_TRUE(launched_callback);
  EXPECT_TRUE(terminated_callback);
  RegisterOnlyLaunch(bootstrap_aa_thethird_, std::move(host_aa_unique_));
  EXPECT_TRUE(host_aa_->HasBootstrapConnected());
}

TEST_F(AppShimManagerTest, MaybeTerminate) {
  // Launch shims, adding entries in the map.
  RegisterOnlyLaunch(bootstrap_aa_, std::move(host_aa_unique_));
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kSuccess,
            *bootstrap_aa_result_);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));

  RegisterOnlyLaunch(bootstrap_ab_, std::move(host_ab_unique_));
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kSuccess,
            *bootstrap_ab_result_);
  EXPECT_EQ(host_ab_.get(), manager_->FindHost(&profile_a_, kTestAppIdB));

  // Quitting when there's another shim should not terminate.
  EXPECT_CALL(*manager_, MaybeTerminate()).Times(0);
  manager_->OnAppDeactivated(&profile_a_, kTestAppIdA);

  // Quitting when it's the last shim should terminate.
  EXPECT_CALL(*manager_, MaybeTerminate());
  manager_->OnAppDeactivated(&profile_a_, kTestAppIdB);
}

TEST_F(AppShimManagerTest, RegisterOnly) {
  // For an chrome::mojom::AppShimLaunchType::kRegisterOnly, don't launch the
  // app.
  EXPECT_CALL(*delegate_, LaunchApp(_, _, _)).Times(0);
  RegisterOnlyLaunch(bootstrap_aa_, std::move(host_aa_unique_));
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kSuccess,
            *bootstrap_aa_result_);
  EXPECT_TRUE(manager_->FindHost(&profile_a_, kTestAppIdA));

  // Close the shim, removing the entry in the map.
  manager_->OnShimProcessDisconnected(host_aa_.get());
  EXPECT_FALSE(manager_->FindHost(&profile_a_, kTestAppIdA));
}

TEST_F(AppShimManagerTest, DontCreateHost) {
  delegate_->SetAppCanCreateHost(false);

  // The app should be launched.
  EXPECT_CALL(*delegate_, LaunchApp(_, _, _)).Times(1);
  NormalLaunch(bootstrap_ab_, std::move(host_ab_unique_));
  // But the bootstrap should be closed.
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kSuccessAndDisconnect,
            *bootstrap_ab_result_);
  // And we should create no host.
  EXPECT_FALSE(manager_->FindHost(&profile_a_, kTestAppIdB));
}

TEST_F(AppShimManagerTest, LoadProfile) {
  // If the profile is not loaded when an OnShimProcessConnected arrives, return
  // false and load the profile asynchronously. Launch the app when the profile
  // is ready.
  EXPECT_CALL(*manager_, ProfileForPath(profile_path_a_))
      .WillOnce(Return(static_cast<Profile*>(nullptr)))
      .WillRepeatedly(Return(&profile_a_));
  NormalLaunch(bootstrap_aa_, std::move(host_aa_unique_));
  EXPECT_FALSE(manager_->FindHost(&profile_a_, kTestAppIdA));
  manager_->RunLoadProfileCallback(profile_path_a_, &profile_a_);
  EXPECT_TRUE(manager_->FindHost(&profile_a_, kTestAppIdA));
}

// Tests that calls to OnShimFocus, OnShimHide correctly handle a null extension
// being provided by the extension system.
TEST_F(AppShimManagerTest, ExtensionUninstalled) {
  LaunchAndActivate(bootstrap_aa_, std::move(host_aa_unique_), &profile_a_);

  ShimNormalFocus(host_aa_.get());
  EXPECT_NE(nullptr, host_aa_.get());

  // Set up the mock to return a null extension, as if it were uninstalled.
  EXPECT_CALL(*delegate_, AppIsInstalled(&profile_a_, kTestAppIdA))
      .WillRepeatedly(Return(false));

  // Now trying to focus should automatically close the shim, and not try to
  // get the window list.
  ShimNormalFocus(host_aa_.get());
  EXPECT_EQ(nullptr, host_aa_.get());
}

TEST_F(AppShimManagerTest, PreExistingHost) {
  // Create a host for our profile.
  manager_->SetHostForCreate(std::move(host_aa_unique_));
  EXPECT_EQ(nullptr, manager_->FindHost(&profile_a_, kTestAppIdA));
  EXPECT_CALL(*delegate_, DoLaunchShim(&profile_a_, kTestAppIdA, false))
      .Times(1);
  manager_->OnAppActivated(&profile_a_, kTestAppIdA);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));
  EXPECT_FALSE(host_aa_->did_connect_to_host());

  // Launch the app for this host. It should find the pre-existing host, and the
  // pre-existing host's launch result should be set.
  EXPECT_CALL(*manager_, OnShimFocus(host_aa_.get())).Times(1);
  EXPECT_CALL(*delegate_, LaunchApp(&profile_a_, kTestAppIdA, _)).Times(0);
  EXPECT_FALSE(host_aa_->did_connect_to_host());
  DoShimLaunch(bootstrap_aa_, nullptr,
               chrome::mojom::AppShimLaunchType::kRegisterOnly,
               std::vector<base::FilePath>());
  EXPECT_TRUE(host_aa_->did_connect_to_host());
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kSuccess,
            *bootstrap_aa_result_);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));

  // Try to launch the app again. It should fail to launch, and the previous
  // profile should remain.
  DoShimLaunch(bootstrap_aa_duplicate_, nullptr,
               chrome::mojom::AppShimLaunchType::kRegisterOnly,
               std::vector<base::FilePath>());
  EXPECT_TRUE(host_aa_->did_connect_to_host());
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kDuplicateHost,
            *bootstrap_aa_duplicate_result_);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));
}

TEST_F(AppShimManagerTest, MultiProfile) {
  // Test with a bookmark app (host is shared).
  {
    // Create a host for profile A.
    manager_->SetHostForCreate(std::move(host_aa_unique_));
    EXPECT_EQ(nullptr, manager_->FindHost(&profile_a_, kTestAppIdA));
    manager_->OnAppActivated(&profile_a_, kTestAppIdA);
    EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));
    EXPECT_FALSE(host_aa_->did_connect_to_host());

    // Ensure that profile B has the same host.
    manager_->SetHostForCreate(std::move(host_ba_unique_));
    EXPECT_EQ(nullptr, manager_->FindHost(&profile_b_, kTestAppIdA));
    manager_->OnAppActivated(&profile_b_, kTestAppIdA);
    EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_b_, kTestAppIdA));
    EXPECT_FALSE(host_aa_->did_connect_to_host());
  }

  // Test with a non-bookmark app (host is not shared).
  {
    // Create a host for profile A.
    manager_->SetHostForCreate(std::move(host_ab_unique_));
    EXPECT_EQ(nullptr, manager_->FindHost(&profile_a_, kTestAppIdB));
    manager_->OnAppActivated(&profile_a_, kTestAppIdB);
    EXPECT_EQ(host_ab_.get(), manager_->FindHost(&profile_a_, kTestAppIdB));
    EXPECT_FALSE(host_ab_->did_connect_to_host());

    // Ensure that profile B has the same host.
    manager_->SetHostForCreate(std::move(host_bb_unique_));
    EXPECT_EQ(nullptr, manager_->FindHost(&profile_b_, kTestAppIdB));
    manager_->OnAppActivated(&profile_b_, kTestAppIdB);
    EXPECT_EQ(host_bb_.get(), manager_->FindHost(&profile_b_, kTestAppIdB));
    EXPECT_FALSE(host_bb_->did_connect_to_host());
  }
}

TEST_F(AppShimManagerTest, MultiProfileShimLaunch) {
  manager_->SetHostForCreate(std::move(host_aa_unique_));
  ShimLaunchedCallback launched_callback;
  delegate_->SetCaptureShimLaunchedCallback(&launched_callback);
  ShimTerminatedCallback terminated_callback;
  delegate_->SetCaptureShimTerminatedCallback(&terminated_callback);

  // Launch the app for profile A. This should trigger a shim launch request.
  EXPECT_CALL(*delegate_, DoLaunchShim(&profile_a_, kTestAppIdA,
                                       false /* recreate_shim */));
  EXPECT_EQ(nullptr, manager_->FindHost(&profile_a_, kTestAppIdA));
  manager_->OnAppActivated(&profile_a_, kTestAppIdA);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));
  EXPECT_FALSE(host_aa_->did_connect_to_host());

  // Launch the app for profile B. This should not cause a shim launch request.
  EXPECT_CALL(*delegate_, DoLaunchShim(_, _, _)).Times(0);
  manager_->OnAppActivated(&profile_b_, kTestAppIdA);

  // Indicate the profile A that its launch succeeded.
  EXPECT_TRUE(launched_callback);
  EXPECT_TRUE(terminated_callback);
  std::move(launched_callback).Run(base::Process(5));
  EXPECT_FALSE(launched_callback);
  EXPECT_TRUE(terminated_callback);
}

TEST_F(AppShimManagerTest, MultiProfileSelectMenu) {
  manager_->SetHostForCreate(std::move(host_aa_unique_));
  ShimLaunchedCallback launched_callback;
  delegate_->SetCaptureShimLaunchedCallback(&launched_callback);
  ShimTerminatedCallback terminated_callback;
  delegate_->SetCaptureShimTerminatedCallback(&terminated_callback);

  // Launch the app for profile A. This should trigger a shim launch request.
  EXPECT_CALL(*delegate_, DoLaunchShim(&profile_a_, kTestAppIdA,
                                       false /* recreate_shim */));
  EXPECT_EQ(nullptr, manager_->FindHost(&profile_a_, kTestAppIdA));
  manager_->OnAppActivated(&profile_a_, kTestAppIdA);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));
  EXPECT_FALSE(host_aa_->did_connect_to_host());

  // Indicate the profile A that its launch succeeded.
  EXPECT_TRUE(launched_callback);
  EXPECT_TRUE(terminated_callback);
  std::move(launched_callback).Run(base::Process(5));
  EXPECT_FALSE(launched_callback);
  EXPECT_TRUE(terminated_callback);

  // Select profile B from the menu. This should request that the app be
  // launched.
  EXPECT_CALL(*delegate_, LaunchApp(&profile_b_, kTestAppIdA, _));
  host_aa_->ProfileSelectedFromMenu(profile_path_b_);
  EXPECT_CALL(*delegate_, DoLaunchShim(_, _, _)).Times(0);
  manager_->OnAppActivated(&profile_b_, kTestAppIdA);

  // Select profile A and B from the menu -- this should not request a launch,
  // because the profiles are already enabled.
  EXPECT_CALL(*delegate_, LaunchApp(_, _, _)).Times(0);
  host_aa_->ProfileSelectedFromMenu(profile_path_a_);
  host_aa_->ProfileSelectedFromMenu(profile_path_b_);
}

TEST_F(AppShimManagerTest, ProfileMenuOneProfile) {
  {
    auto item_a = chrome::mojom::ProfileMenuItem::New();
    item_a->profile_path = profile_path_a_;
    item_a->menu_index = 999;

    std::vector<chrome::mojom::ProfileMenuItemPtr> items;
    items.push_back(std::move(item_a));
    manager_->SetProfileMenuItems(std::move(items));
  }

  // Set this app to be installed for profile A.
  AppShimRegistry::Get()->OnAppInstalledForProfile(kTestAppIdA,
                                                   profile_path_a_);

  // When the app activates, a host is created. This will trigger building
  // the avatar menu.
  manager_->SetHostForCreate(std::move(host_aa_unique_));
  EXPECT_CALL(*delegate_, DoLaunchShim(&profile_a_, kTestAppIdA, false));
  manager_->OnAppActivated(&profile_a_, kTestAppIdA);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));

  // Launch the shim.
  EXPECT_CALL(*delegate_, LaunchApp(&profile_a_, kTestAppIdA, _)).Times(0);
  RegisterOnlyLaunch(bootstrap_aa_, nullptr);
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kSuccess,
            *bootstrap_aa_result_);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));
  const auto& menu_items = host_aa_->test_app_shim_->profile_menu_items_;

  // We should have no menu items, because there is only one installed profile.
  EXPECT_TRUE(menu_items.empty());

  // Add profile B to the avatar menu and call the avatar menu observer update
  // method.
  {
    auto item_a = chrome::mojom::ProfileMenuItem::New();
    item_a->profile_path = profile_path_a_;
    item_a->menu_index = 999;

    auto item_b = chrome::mojom::ProfileMenuItem::New();
    item_b->profile_path = profile_path_b_;
    item_b->menu_index = 111;

    std::vector<chrome::mojom::ProfileMenuItemPtr> items;
    items.push_back(std::move(item_a));
    items.push_back(std::move(item_b));
    manager_->SetProfileMenuItems(std::move(items));
  }

  // We should still only have no menu items, because the app is not installed
  // for multiple profiles.
  EXPECT_TRUE(menu_items.empty());

  // Now install for profile B.
  AppShimRegistry::Get()->OnAppInstalledForProfile(kTestAppIdA,
                                                   profile_path_b_);
  manager_->OnAppActivated(&profile_b_, kTestAppIdA);
  EXPECT_EQ(menu_items.size(), 2u);
  EXPECT_EQ(menu_items[0]->profile_path, profile_path_b_);
  EXPECT_EQ(menu_items[1]->profile_path, profile_path_a_);
}

TEST_F(AppShimManagerTest, FindProfileFromBadProfile) {
  // Set this app to be installed for profile A and B.
  AppShimRegistry::Get()->OnAppInstalledForProfile(kTestAppIdA,
                                                   profile_path_a_);
  AppShimRegistry::Get()->OnAppInstalledForProfile(kTestAppIdA,
                                                   profile_path_b_);

  // Set the app to be last-active on profile A.
  std::set<base::FilePath> last_active_profile_paths;
  last_active_profile_paths.insert(profile_path_a_);
  AppShimRegistry::Get()->OnAppQuit(kTestAppIdA, last_active_profile_paths);

  // Launch the shim requesting profile C.
  manager_->SetHostForCreate(std::move(host_aa_unique_));
  EXPECT_CALL(*delegate_, LaunchApp(&profile_a_, kTestAppIdA, _)).Times(1);
  EXPECT_CALL(*delegate_, LaunchApp(&profile_b_, kTestAppIdA, _)).Times(0);
  EXPECT_CALL(*delegate_, EnableExtension(&profile_c_, kTestAppIdA, _))
      .WillOnce(RunOnceCallback<2>());
  NormalLaunch(bootstrap_ca_, nullptr);
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kSuccess,
            *bootstrap_ca_result_);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));
}

TEST_F(AppShimManagerTest, FindProfileFromNoProfile) {
  // Set this app to be installed for profile A.
  AppShimRegistry::Get()->OnAppInstalledForProfile(kTestAppIdA,
                                                   profile_path_a_);

  // Launch the shim without specifying a profile.
  manager_->SetHostForCreate(std::move(host_aa_unique_));
  EXPECT_CALL(*delegate_, LaunchApp(&profile_a_, kTestAppIdA, _)).Times(1);
  EXPECT_CALL(*delegate_, LaunchApp(&profile_b_, kTestAppIdA, _)).Times(0);
  NormalLaunch(bootstrap_xa_, nullptr);
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kSuccess,
            *bootstrap_xa_result_);
  EXPECT_EQ(host_aa_.get(), manager_->FindHost(&profile_a_, kTestAppIdA));
}
}  // namespace apps
