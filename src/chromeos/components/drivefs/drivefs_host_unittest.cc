// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/drivefs/drivefs_host.h"

#include <set>
#include <type_traits>
#include <utility>

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/simple_test_clock.h"
#include "base/timer/mock_timer.h"
#include "chromeos/components/drivefs/drivefs_host_observer.h"
#include "chromeos/components/drivefs/pending_connection_manager.h"
#include "chromeos/disks/mock_disk_mount_manager.h"
#include "components/drive/drive_notification_manager.h"
#include "components/drive/drive_notification_observer.h"
#include "components/invalidation/impl/fake_invalidation_service.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "net/base/mock_network_change_notifier.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drivefs {
namespace {

using testing::_;
using MountFailure = DriveFsHost::MountObserver::MountFailure;

class TestingMojoConnectionDelegate
    : public DriveFsHost::MojoConnectionDelegate {
 public:
  TestingMojoConnectionDelegate(
      mojom::DriveFsBootstrapPtrInfo pending_bootstrap)
      : pending_bootstrap_(std::move(pending_bootstrap)) {}

  mojom::DriveFsBootstrapPtrInfo InitializeMojoConnection() override {
    return std::move(pending_bootstrap_);
  }

  void AcceptMojoConnection(base::ScopedFD handle) override {}

 private:
  mojom::DriveFsBootstrapPtrInfo pending_bootstrap_;
};

class MockDriveFs : public mojom::DriveFsInterceptorForTesting,
                    public mojom::SearchQuery {
 public:
  MockDriveFs() : search_binding_(this) {}

  DriveFs* GetForwardingInterface() override {
    NOTREACHED();
    return nullptr;
  }

  void FetchChangeLog(std::vector<mojom::FetchChangeLogOptionsPtr> options) {
    std::vector<std::pair<int64_t, std::string>> unwrapped_options;
    for (auto& entry : options) {
      unwrapped_options.push_back(
          std::make_pair(entry->change_id, entry->team_drive_id));
    }
    FetchChangeLogImpl(unwrapped_options);
  }

  MOCK_METHOD1(
      FetchChangeLogImpl,
      void(const std::vector<std::pair<int64_t, std::string>>& options));
  MOCK_METHOD0(FetchAllChangeLogs, void());

  MOCK_CONST_METHOD1(OnStartSearchQuery, void(const mojom::QueryParameters&));
  void StartSearchQuery(mojom::SearchQueryRequest query,
                        mojom::QueryParametersPtr query_params) override {
    if (search_binding_.is_bound())
      search_binding_.Unbind();
    OnStartSearchQuery(*query_params);
    search_binding_.Bind(std::move(query));
  }

  MOCK_METHOD1(OnGetNextPage,
               drive::FileError(
                   base::Optional<std::vector<mojom::QueryItemPtr>>* items));

  void GetNextPage(GetNextPageCallback callback) override {
    base::Optional<std::vector<mojom::QueryItemPtr>> items;
    auto error = OnGetNextPage(&items);
    std::move(callback).Run(error, std::move(items));
  }

 private:
  mojo::Binding<mojom::SearchQuery> search_binding_;
};

class TestingDriveFsHostDelegate : public DriveFsHost::Delegate,
                                   public DriveFsHost::MountObserver {
 public:
  TestingDriveFsHostDelegate(
      std::unique_ptr<service_manager::Connector> connector,
      const AccountId& account_id)
      : connector_(std::move(connector)),
        account_id_(account_id),
        drive_notification_manager_(&invalidation_service_) {}

  ~TestingDriveFsHostDelegate() override {
    drive_notification_manager_.Shutdown();
  }

  void set_pending_bootstrap(mojom::DriveFsBootstrapPtrInfo pending_bootstrap) {
    pending_bootstrap_ = std::move(pending_bootstrap);
  }

  // DriveFsHost::MountObserver:
  MOCK_METHOD1(OnMounted, void(const base::FilePath&));
  MOCK_METHOD2(OnMountFailed,
               void(MountFailure, base::Optional<base::TimeDelta>));
  MOCK_METHOD1(OnUnmounted, void(base::Optional<base::TimeDelta>));

  drive::DriveNotificationManager& GetDriveNotificationManager() override {
    return drive_notification_manager_;
  }

 private:
  // DriveFsHost::Delegate:
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      override {
    return nullptr;
  }
  service_manager::Connector* GetConnector() override {
    return connector_.get();
  }
  const AccountId& GetAccountId() override { return account_id_; }
  std::string GetObfuscatedAccountId() override {
    return "salt-" + account_id_.GetAccountIdKey();
  }

  std::unique_ptr<DriveFsHost::MojoConnectionDelegate>
  CreateMojoConnectionDelegate() override {
    DCHECK(pending_bootstrap_);
    return std::make_unique<TestingMojoConnectionDelegate>(
        std::move(pending_bootstrap_));
  }

  const std::unique_ptr<service_manager::Connector> connector_;
  const AccountId account_id_;
  mojom::DriveFsBootstrapPtrInfo pending_bootstrap_;
  invalidation::FakeInvalidationService invalidation_service_;
  drive::DriveNotificationManager drive_notification_manager_;

  DISALLOW_COPY_AND_ASSIGN(TestingDriveFsHostDelegate);
};

class MockIdentityManager {
 public:
  explicit MockIdentityManager(const base::Clock* clock) : clock_(clock) {}
  MOCK_METHOD3(
      GetAccessToken,
      std::pair<base::Optional<std::string>, GoogleServiceAuthError::State>(
          const std::string& account_id,
          const ::identity::ScopeSet& scopes,
          const std::string& consumer_id));

  void OnGetAccessToken(
      const std::string& account_id,
      const ::identity::ScopeSet& scopes,
      const std::string& consumer_id,
      identity::mojom::IdentityManager::GetAccessTokenCallback callback) {
    if (pause_requests_) {
      callbacks_.push_back(std::move(callback));
      return;
    }
    auto result = GetAccessToken(account_id, scopes, consumer_id);
    std::move(callback).Run(std::move(result.first),
                            clock_->Now() + base::TimeDelta::FromHours(1),
                            GoogleServiceAuthError(result.second));
  }

  std::vector<identity::mojom::IdentityManager::GetAccessTokenCallback>&
  callbacks() {
    return callbacks_;
  }

  void set_pause_requests(bool pause) { pause_requests_ = pause; }

  const base::Clock* const clock_;
  bool pause_requests_ = false;
  std::vector<identity::mojom::IdentityManager::GetAccessTokenCallback>
      callbacks_;
  mojo::BindingSet<identity::mojom::IdentityManager>* bindings_ = nullptr;
};

class FakeIdentityService
    : public identity::mojom::IdentityManagerInterceptorForTesting,
      public service_manager::Service {
 public:
  explicit FakeIdentityService(MockIdentityManager* mock) : mock_(mock) {
    binder_registry_.AddInterface(
        base::BindRepeating(&FakeIdentityService::BindIdentityManagerRequest,
                            base::Unretained(this)));
    mock_->bindings_ = &bindings_;
  }

  ~FakeIdentityService() override { mock_->bindings_ = nullptr; }

 private:
  void OnBindInterface(const service_manager::BindSourceInfo& source,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    binder_registry_.BindInterface(interface_name, std::move(interface_pipe));
  }

  void BindIdentityManagerRequest(
      identity::mojom::IdentityManagerRequest request) {
    bindings_.AddBinding(this, std::move(request));
  }

  // identity::mojom::IdentityManagerInterceptorForTesting overrides:
  void GetPrimaryAccountWhenAvailable(
      GetPrimaryAccountWhenAvailableCallback callback) override {
    auto account_id = AccountId::FromUserEmailGaiaId("test@example.com", "ID");
    AccountInfo account_info;
    account_info.email = account_id.GetUserEmail();
    account_info.gaia = account_id.GetGaiaId();
    account_info.account_id = account_id.GetAccountIdKey();
    std::move(callback).Run(account_info, {});
  }

  void GetAccessToken(const std::string& account_id,
                      const ::identity::ScopeSet& scopes,
                      const std::string& consumer_id,
                      GetAccessTokenCallback callback) override {
    mock_->OnGetAccessToken(account_id, scopes, consumer_id,
                            std::move(callback));
  }

  IdentityManager* GetForwardingInterface() override {
    NOTREACHED();
    return nullptr;
  }

  MockIdentityManager* const mock_;
  service_manager::BinderRegistry binder_registry_;
  mojo::BindingSet<identity::mojom::IdentityManager> bindings_;

  DISALLOW_COPY_AND_ASSIGN(FakeIdentityService);
};

class MockDriveFsHostObserver : public DriveFsHostObserver {
 public:
  MOCK_METHOD0(OnUnmounted, void());
  MOCK_METHOD1(OnSyncingStatusUpdate, void(const mojom::SyncingStatus& status));
  MOCK_METHOD1(OnFilesChanged,
               void(const std::vector<mojom::FileChange>& changes));
  MOCK_METHOD1(OnError, void(const mojom::DriveError& error));
};

ACTION_P(RunQuitClosure, quit) {
  std::move(*quit).Run();
}

class DriveFsHostTest : public ::testing::Test, public mojom::DriveFsBootstrap {
 public:
  DriveFsHostTest()
      : mock_identity_manager_(&clock_),
        bootstrap_binding_(this),
        binding_(&mock_drivefs_) {
    clock_.SetNow(base::Time::Now());
  }

 protected:
  void SetUp() override {
    testing::Test::SetUp();
    profile_path_ = base::FilePath(FILE_PATH_LITERAL("/path/to/profile"));
    account_id_ = AccountId::FromUserEmailGaiaId("test@example.com", "ID");

    disk_manager_ = std::make_unique<chromeos::disks::MockDiskMountManager>();
    connector_factory_ =
        service_manager::TestConnectorFactory::CreateForUniqueService(
            std::make_unique<FakeIdentityService>(&mock_identity_manager_));
    host_delegate_ = std::make_unique<TestingDriveFsHostDelegate>(
        connector_factory_->CreateConnector(), account_id_);
    auto timer = std::make_unique<base::MockOneShotTimer>();
    timer_ = timer.get();
    host_ = std::make_unique<DriveFsHost>(
        profile_path_, host_delegate_.get(), host_delegate_.get(), &clock_,
        disk_manager_.get(), std::move(timer));
  }

  void TearDown() override {
    host_.reset();
    disk_manager_.reset();
  }

  void DispatchMountEvent(
      chromeos::disks::DiskMountManager::MountEvent event,
      chromeos::MountError error_code,
      const chromeos::disks::DiskMountManager::MountPointInfo& mount_info) {
    disk_manager_->NotifyMountEvent(event, error_code, mount_info);
  }

  std::string StartMount() {
    std::string source;
    EXPECT_CALL(
        *disk_manager_,
        MountPath(
            testing::StartsWith("drivefs://"), "", "drivefs-salt-g-ID",
            testing::Contains("datadir=/path/to/profile/GCache/v2/salt-g-ID"),
            _, chromeos::MOUNT_ACCESS_MODE_READ_WRITE))
        .WillOnce(testing::SaveArg<0>(&source));

    mojom::DriveFsBootstrapPtrInfo bootstrap;
    bootstrap_binding_.Bind(mojo::MakeRequest(&bootstrap));
    host_delegate_->set_pending_bootstrap(std::move(bootstrap));
    pending_delegate_request_ = mojo::MakeRequest(&delegate_ptr_);

    EXPECT_TRUE(host_->Mount());
    testing::Mock::VerifyAndClear(&disk_manager_);

    return source.substr(strlen("drivefs://"));
  }

  void DispatchMountSuccessEvent(const std::string& token) {
    DispatchMountEvent(chromeos::disks::DiskMountManager::MOUNTING,
                       chromeos::MOUNT_ERROR_NONE,
                       {base::StrCat({"drivefs://", token}),
                        "/media/drivefsroot/salt-g-ID",
                        chromeos::MOUNT_TYPE_NETWORK_STORAGE,
                        {}});
  }

  void SendOnMounted() { delegate_ptr_->OnMounted(); }

  void SendOnUnmounted(base::Optional<base::TimeDelta> delay) {
    delegate_ptr_->OnUnmounted(std::move(delay));
  }

  void SendMountFailed(base::Optional<base::TimeDelta> delay) {
    delegate_ptr_->OnMountFailed(std::move(delay));
  }

  void EstablishConnection() {
    token_ = StartMount();
    DispatchMountSuccessEvent(token_);

    ASSERT_TRUE(PendingConnectionManager::Get().OpenIpcChannel(token_, {}));
    {
      base::RunLoop run_loop;
      bootstrap_binding_.set_connection_error_handler(run_loop.QuitClosure());
      run_loop.Run();
    }
  }

  void DoMount() {
    EstablishConnection();
    base::RunLoop run_loop;
    base::OnceClosure quit_closure = run_loop.QuitClosure();
    EXPECT_CALL(*host_delegate_,
                OnMounted(base::FilePath("/media/drivefsroot/salt-g-ID")))
        .WillOnce(RunQuitClosure(&quit_closure));
    // Eventually we must attempt unmount.
    EXPECT_CALL(*disk_manager_, UnmountPath("/media/drivefsroot/salt-g-ID",
                                            chromeos::UNMOUNT_OPTIONS_NONE, _));
    SendOnMounted();
    run_loop.Run();
    ASSERT_TRUE(host_->IsMounted());
  }

  void DoUnmount() {
    host_->Unmount();
    binding_.Unbind();
    bootstrap_binding_.Unbind();
    delegate_ptr_.reset();
    base::RunLoop().RunUntilIdle();
    testing::Mock::VerifyAndClearExpectations(disk_manager_.get());
  }

  void ExpectAccessToken(mojom::AccessTokenStatus expected_status,
                         const std::string& expected_token) {
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();
    delegate_ptr_->GetAccessToken(
        "client ID", "app ID", {"scope1", "scope2"},
        base::BindLambdaForTesting(
            [&](mojom::AccessTokenStatus status, const std::string& token) {
              EXPECT_EQ(expected_status, status);
              EXPECT_EQ(expected_token, token);
              std::move(quit_closure).Run();
            }));
    run_loop.Run();
  }

  void Init(mojom::DriveFsConfigurationPtr config,
            mojom::DriveFsRequest drive_fs_request,
            mojom::DriveFsDelegatePtr delegate) override {
    EXPECT_EQ("test@example.com", config->user_email);
    init_access_token_ = std::move(config->access_token);
    binding_.Bind(std::move(drive_fs_request));
    mojo::FuseInterface(std::move(pending_delegate_request_),
                        delegate.PassInterface());
  }

  base::FilePath profile_path_;
  base::test::ScopedTaskEnvironment task_environment_;
  AccountId account_id_;
  std::unique_ptr<chromeos::disks::MockDiskMountManager> disk_manager_;
  base::SimpleTestClock clock_;
  MockIdentityManager mock_identity_manager_;
  std::unique_ptr<service_manager::TestConnectorFactory> connector_factory_;
  std::unique_ptr<TestingDriveFsHostDelegate> host_delegate_;
  std::unique_ptr<DriveFsHost> host_;
  base::MockOneShotTimer* timer_;
  net::test::MockNetworkChangeNotifier network_;

  mojo::Binding<mojom::DriveFsBootstrap> bootstrap_binding_;
  MockDriveFs mock_drivefs_;
  mojo::Binding<mojom::DriveFs> binding_;
  mojom::DriveFsDelegatePtr delegate_ptr_;
  mojom::DriveFsDelegateRequest pending_delegate_request_;
  std::string token_;
  base::Optional<std::string> init_access_token_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DriveFsHostTest);
};

TEST_F(DriveFsHostTest, Basic) {
  EXPECT_FALSE(host_->IsMounted());

  EXPECT_EQ(base::FilePath("/path/to/profile/GCache/v2/salt-g-ID"),
            host_->GetDataPath());

  ASSERT_NO_FATAL_FAILURE(DoMount());
  EXPECT_FALSE(init_access_token_);

  EXPECT_EQ(base::FilePath("/media/drivefsroot/salt-g-ID"),
            host_->GetMountPath());

  DoUnmount();
}

TEST_F(DriveFsHostTest, GetMountPathWhileUnmounted) {
  EXPECT_EQ(base::FilePath("/media/fuse/drivefs-salt-g-ID"),
            host_->GetMountPath());
}

TEST_F(DriveFsHostTest, OnMountedBeforeMountEvent) {
  auto token = StartMount();
  ASSERT_TRUE(PendingConnectionManager::Get().OpenIpcChannel(token, {}));
  SendOnMounted();
  EXPECT_CALL(*host_delegate_, OnMounted(_)).Times(0);
  delegate_ptr_.FlushForTesting();

  testing::Mock::VerifyAndClear(host_delegate_.get());

  EXPECT_FALSE(host_->IsMounted());

  EXPECT_CALL(*host_delegate_,
              OnMounted(base::FilePath("/media/drivefsroot/salt-g-ID")));
  EXPECT_CALL(*disk_manager_, UnmountPath("/media/drivefsroot/salt-g-ID",
                                          chromeos::UNMOUNT_OPTIONS_NONE, _));

  DispatchMountSuccessEvent(token);

  ASSERT_TRUE(host_->IsMounted());
  EXPECT_EQ(base::FilePath("/media/drivefsroot/salt-g-ID"),
            host_->GetMountPath());

  DoUnmount();
}

TEST_F(DriveFsHostTest, OnMountFailedFromMojo) {
  ASSERT_FALSE(host_->IsMounted());

  ASSERT_NO_FATAL_FAILURE(EstablishConnection());
  base::RunLoop run_loop;
  base::OnceClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*host_delegate_, OnMountFailed(MountFailure::kUnknown, _))
      .WillOnce(RunQuitClosure(&quit_closure));
  SendMountFailed({});
  run_loop.Run();
  ASSERT_FALSE(host_->IsMounted());
}

TEST_F(DriveFsHostTest, OnMountFailedFromDbus) {
  ASSERT_FALSE(host_->IsMounted());

  auto token = StartMount();

  base::RunLoop run_loop;
  base::OnceClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*host_delegate_, OnMountFailed(MountFailure::kInvocation, _))
      .WillOnce(RunQuitClosure(&quit_closure));
  DispatchMountEvent(chromeos::disks::DiskMountManager::MOUNTING,
                     chromeos::MOUNT_ERROR_INVALID_MOUNT_OPTIONS,
                     {base::StrCat({"drivefs://", token}),
                      "/media/drivefsroot/salt-g-ID",
                      chromeos::MOUNT_TYPE_NETWORK_STORAGE,
                      {}});
  run_loop.Run();

  ASSERT_FALSE(host_->IsMounted());
}

TEST_F(DriveFsHostTest, OnMountFailed_UnmountInObserver) {
  ASSERT_FALSE(host_->IsMounted());

  auto token = StartMount();

  base::RunLoop run_loop;
  base::OnceClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(*host_delegate_, OnMountFailed(MountFailure::kInvocation, _))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        std::move(quit_closure).Run();
        host_->Unmount();
      }));
  DispatchMountEvent(chromeos::disks::DiskMountManager::MOUNTING,
                     chromeos::MOUNT_ERROR_INVALID_MOUNT_OPTIONS,
                     {base::StrCat({"drivefs://", token}),
                      "/media/drivefsroot/salt-g-ID",
                      chromeos::MOUNT_TYPE_NETWORK_STORAGE,
                      {}});
  run_loop.Run();

  ASSERT_FALSE(host_->IsMounted());
}

TEST_F(DriveFsHostTest, UnmountAfterMountComplete) {
  MockDriveFsHostObserver observer;
  ScopedObserver<DriveFsHost, DriveFsHostObserver> observer_scoper(&observer);
  observer_scoper.Add(host_.get());

  ASSERT_NO_FATAL_FAILURE(DoMount());

  EXPECT_CALL(observer, OnUnmounted());
  base::RunLoop run_loop;
  delegate_ptr_.set_connection_error_handler(run_loop.QuitClosure());
  host_->Unmount();
  run_loop.Run();
}

TEST_F(DriveFsHostTest, UnmountBeforeMountEvent) {
  MockDriveFsHostObserver observer;
  ScopedObserver<DriveFsHost, DriveFsHostObserver> observer_scoper(&observer);
  observer_scoper.Add(host_.get());
  EXPECT_CALL(observer, OnUnmounted()).Times(0);

  auto token = StartMount();
  EXPECT_FALSE(host_->IsMounted());
  host_->Unmount();
  EXPECT_FALSE(PendingConnectionManager::Get().OpenIpcChannel(token, {}));
}

TEST_F(DriveFsHostTest, UnmountBeforeMojoConnection) {
  MockDriveFsHostObserver observer;
  ScopedObserver<DriveFsHost, DriveFsHostObserver> observer_scoper(&observer);
  observer_scoper.Add(host_.get());
  EXPECT_CALL(observer, OnUnmounted()).Times(0);

  auto token = StartMount();
  DispatchMountSuccessEvent(token);

  EXPECT_FALSE(host_->IsMounted());
  EXPECT_CALL(*disk_manager_, UnmountPath("/media/drivefsroot/salt-g-ID",
                                          chromeos::UNMOUNT_OPTIONS_NONE, _));

  host_->Unmount();
  EXPECT_FALSE(PendingConnectionManager::Get().OpenIpcChannel(token, {}));
}

TEST_F(DriveFsHostTest, DestroyBeforeMountEvent) {
  auto token = StartMount();
  EXPECT_CALL(*disk_manager_, UnmountPath(_, _, _)).Times(0);

  host_.reset();
  EXPECT_FALSE(PendingConnectionManager::Get().OpenIpcChannel(token, {}));
}

TEST_F(DriveFsHostTest, DestroyBeforeMojoConnection) {
  auto token = StartMount();
  DispatchMountSuccessEvent(token);
  EXPECT_CALL(*disk_manager_, UnmountPath("/media/drivefsroot/salt-g-ID",
                                          chromeos::UNMOUNT_OPTIONS_NONE, _));

  host_.reset();
  EXPECT_FALSE(PendingConnectionManager::Get().OpenIpcChannel(token, {}));
}

TEST_F(DriveFsHostTest, ObserveOtherMount) {
  auto token = StartMount();
  EXPECT_CALL(*disk_manager_, UnmountPath(_, _, _)).Times(0);

  DispatchMountEvent(chromeos::disks::DiskMountManager::MOUNTING,
                     chromeos::MOUNT_ERROR_DIRECTORY_CREATION_FAILED,
                     {"some/other/mount/event",
                      "/some/other/mount/point",
                      chromeos::MOUNT_TYPE_DEVICE,
                      {}});
  DispatchMountEvent(chromeos::disks::DiskMountManager::UNMOUNTING,
                     chromeos::MOUNT_ERROR_NONE,
                     {base::StrCat({"drivefs://", token}),
                      "/media/drivefsroot/salt-g-ID",
                      chromeos::MOUNT_TYPE_NETWORK_STORAGE,
                      {}});
  EXPECT_FALSE(host_->IsMounted());
  host_->Unmount();
}

TEST_F(DriveFsHostTest, MountError) {
  auto token = StartMount();
  EXPECT_CALL(*disk_manager_, UnmountPath(_, _, _)).Times(0);
  EXPECT_CALL(*host_delegate_, OnMountFailed(MountFailure::kInvocation, _));

  DispatchMountEvent(chromeos::disks::DiskMountManager::MOUNTING,
                     chromeos::MOUNT_ERROR_DIRECTORY_CREATION_FAILED,
                     {base::StrCat({"drivefs://", token}),
                      "/media/drivefsroot/g-ID",
                      chromeos::MOUNT_TYPE_NETWORK_STORAGE,
                      {}});
  EXPECT_FALSE(host_->IsMounted());
  EXPECT_FALSE(PendingConnectionManager::Get().OpenIpcChannel(token, {}));
}

TEST_F(DriveFsHostTest, MountWhileAlreadyMounted) {
  DoMount();
  EXPECT_FALSE(host_->Mount());
}

TEST_F(DriveFsHostTest, UnmountByRemote) {
  ASSERT_NO_FATAL_FAILURE(DoMount());
  base::Optional<base::TimeDelta> delay = base::TimeDelta::FromSeconds(5);
  EXPECT_CALL(*host_delegate_, OnUnmounted(delay));
  SendOnUnmounted(delay);
  base::RunLoop().RunUntilIdle();
}

TEST_F(DriveFsHostTest, BreakConnectionAfterMount) {
  ASSERT_NO_FATAL_FAILURE(DoMount());
  base::Optional<base::TimeDelta> empty;
  EXPECT_CALL(*host_delegate_, OnUnmounted(empty));
  delegate_ptr_.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(DriveFsHostTest, BreakConnectionBeforeMount) {
  auto token = StartMount();
  DispatchMountSuccessEvent(token);
  EXPECT_FALSE(host_->IsMounted());

  base::Optional<base::TimeDelta> empty;
  EXPECT_CALL(*host_delegate_,
              OnMountFailed(MountFailure::kIpcDisconnect, empty));
  delegate_ptr_.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(DriveFsHostTest, MountTimeout) {
  auto token = StartMount();
  DispatchMountSuccessEvent(token);
  EXPECT_FALSE(host_->IsMounted());

  base::Optional<base::TimeDelta> empty;
  EXPECT_CALL(*host_delegate_, OnMountFailed(MountFailure::kTimeout, empty));
  timer_->Fire();
}

// DiskMountManager sometimes sends mount events for all existing mount points.
// Mount events beyond the first should be ignored.
TEST_F(DriveFsHostTest, MultipleMountNotifications) {
  ASSERT_NO_FATAL_FAILURE(DoMount());

  // That is event is ignored is verified the the expectations set in DoMount().
  // OnMounted() should only be invoked once.
  DispatchMountSuccessEvent(token_);
}

TEST_F(DriveFsHostTest, UnsupportedAccountTypes) {
  EXPECT_CALL(*disk_manager_, MountPath(_, _, _, _, _, _)).Times(0);
  const AccountId unsupported_accounts[] = {
      AccountId::FromGaiaId("ID"),
      AccountId::FromUserEmail("test2@example.com"),
      AccountId::AdFromObjGuid("ID"),
  };
  for (auto& account : unsupported_accounts) {
    host_delegate_ = std::make_unique<TestingDriveFsHostDelegate>(
        connector_factory_->CreateConnector(), account);
    host_ = std::make_unique<DriveFsHost>(
        profile_path_, host_delegate_.get(), host_delegate_.get(), &clock_,
        disk_manager_.get(), std::make_unique<base::MockOneShotTimer>());
    EXPECT_FALSE(host_->Mount());
    EXPECT_FALSE(host_->IsMounted());
  }
}

TEST_F(DriveFsHostTest, GetAccessToken_Success) {
  ASSERT_NO_FATAL_FAILURE(DoMount());

  EXPECT_CALL(mock_identity_manager_,
              GetAccessToken("test@example.com", _, "drivefs"))
      .WillOnce(testing::Return(
          std::make_pair("auth token", GoogleServiceAuthError::NONE)));

  ExpectAccessToken(mojom::AccessTokenStatus::kSuccess, "auth token");
}

TEST_F(DriveFsHostTest, GetAccessToken_ParallelRequests) {
  ASSERT_NO_FATAL_FAILURE(DoMount());

  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  delegate_ptr_->GetAccessToken(
      "client ID", "app ID", {"scope1", "scope2"},
      base::BindOnce(
          [](mojom::AccessTokenStatus status, const std::string& token) {
            FAIL() << "Unexpected callback";
          }));
  delegate_ptr_->GetAccessToken(
      "client ID", "app ID", {"scope1", "scope2"},
      base::BindLambdaForTesting(
          [&](mojom::AccessTokenStatus status, const std::string& token) {
            EXPECT_EQ(mojom::AccessTokenStatus::kTransientError, status);
            EXPECT_TRUE(token.empty());
            std::move(quit_closure).Run();
          }));
  run_loop.Run();
}

TEST_F(DriveFsHostTest, GetAccessToken_SequentialRequests) {
  ASSERT_NO_FATAL_FAILURE(DoMount());

  for (int i = 0; i < 3; ++i) {
    EXPECT_CALL(mock_identity_manager_,
                GetAccessToken("test@example.com", _, "drivefs"))
        .WillOnce(testing::Return(
            std::make_pair("auth token", GoogleServiceAuthError::NONE)));
    ExpectAccessToken(mojom::AccessTokenStatus::kSuccess, "auth token");
  }
  for (int i = 0; i < 3; ++i) {
    EXPECT_CALL(mock_identity_manager_,
                GetAccessToken("test@example.com", _, "drivefs"))
        .WillOnce(testing::Return(std::make_pair(
            base::nullopt, GoogleServiceAuthError::ACCOUNT_DISABLED)));
    ExpectAccessToken(mojom::AccessTokenStatus::kAuthError, "");
  }
}

TEST_F(DriveFsHostTest, GetAccessToken_GetAccessTokenFailure_Permanent) {
  ASSERT_NO_FATAL_FAILURE(DoMount());

  EXPECT_CALL(mock_identity_manager_,
              GetAccessToken("test@example.com", _, "drivefs"))
      .WillOnce(testing::Return(std::make_pair(
          base::nullopt, GoogleServiceAuthError::ACCOUNT_DISABLED)));
  ExpectAccessToken(mojom::AccessTokenStatus::kAuthError, "");
}

TEST_F(DriveFsHostTest, GetAccessToken_GetAccessTokenFailure_Transient) {
  ASSERT_NO_FATAL_FAILURE(DoMount());

  EXPECT_CALL(mock_identity_manager_,
              GetAccessToken("test@example.com", _, "drivefs"))
      .WillOnce(testing::Return(std::make_pair(
          base::nullopt, GoogleServiceAuthError::SERVICE_UNAVAILABLE)));
  ExpectAccessToken(mojom::AccessTokenStatus::kTransientError, "");
}

TEST_F(DriveFsHostTest, GetAccessToken_UnmountDuringMojoRequest) {
  ASSERT_NO_FATAL_FAILURE(DoMount());

  EXPECT_CALL(mock_identity_manager_,
              GetAccessToken("test@example.com", _, "drivefs"))
      .WillOnce(testing::DoAll(
          testing::InvokeWithoutArgs([&]() { host_->Unmount(); }),
          testing::Return(std::make_pair(
              base::nullopt, GoogleServiceAuthError::ACCOUNT_DISABLED))));

  base::RunLoop run_loop;
  delegate_ptr_.set_connection_error_handler(run_loop.QuitClosure());
  delegate_ptr_->GetAccessToken(
      "client ID", "app ID", {"scope1", "scope2"},
      base::BindLambdaForTesting([](mojom::AccessTokenStatus status,
                                    const std::string& token) { FAIL(); }));
  run_loop.Run();
  EXPECT_FALSE(host_->IsMounted());

  // Wait for the response to reach the InterfacePtr if it's still open.
  mock_identity_manager_.bindings_->FlushForTesting();
}

ACTION_P(CloneStruct, output) {
  *output = arg0.Clone();
}

TEST_F(DriveFsHostTest, OnSyncingStatusUpdate_ForwardToObservers) {
  ASSERT_NO_FATAL_FAILURE(DoMount());
  MockDriveFsHostObserver observer;
  ScopedObserver<DriveFsHost, DriveFsHostObserver> observer_scoper(&observer);
  observer_scoper.Add(host_.get());
  auto status = mojom::SyncingStatus::New();
  status->item_events.emplace_back(base::in_place, 12, 34, "filename.txt",
                                   mojom::ItemEvent::State::kInProgress, 123,
                                   456);
  mojom::SyncingStatusPtr observed_status;
  EXPECT_CALL(observer, OnSyncingStatusUpdate(_))
      .WillOnce(CloneStruct(&observed_status));
  delegate_ptr_->OnSyncingStatusUpdate(status.Clone());
  delegate_ptr_.FlushForTesting();
  testing::Mock::VerifyAndClear(&observer);

  EXPECT_EQ(status, observed_status);
}

ACTION_P(CloneVectorOfStructs, output) {
  for (auto& s : arg0) {
    output->emplace_back(s.Clone());
  }
}

TEST_F(DriveFsHostTest, OnFilesChanged_ForwardToObservers) {
  ASSERT_NO_FATAL_FAILURE(DoMount());
  MockDriveFsHostObserver observer;
  ScopedObserver<DriveFsHost, DriveFsHostObserver> observer_scoper(&observer);
  observer_scoper.Add(host_.get());
  std::vector<mojom::FileChangePtr> changes;
  changes.emplace_back(base::in_place, base::FilePath("/create"),
                       mojom::FileChange::Type::kCreate);
  changes.emplace_back(base::in_place, base::FilePath("/delete"),
                       mojom::FileChange::Type::kDelete);
  changes.emplace_back(base::in_place, base::FilePath("/modify"),
                       mojom::FileChange::Type::kModify);
  std::vector<mojom::FileChangePtr> observed_changes;
  EXPECT_CALL(observer, OnFilesChanged(_))
      .WillOnce(CloneVectorOfStructs(&observed_changes));
  delegate_ptr_->OnFilesChanged(mojo::Clone(changes));
  delegate_ptr_.FlushForTesting();
  testing::Mock::VerifyAndClear(&observer);

  EXPECT_EQ(changes, observed_changes);
}

TEST_F(DriveFsHostTest, OnError_ForwardToObservers) {
  ASSERT_NO_FATAL_FAILURE(DoMount());
  MockDriveFsHostObserver observer;
  ScopedObserver<DriveFsHost, DriveFsHostObserver> observer_scoper(&observer);
  observer_scoper.Add(host_.get());
  auto error = mojom::DriveError::New(
      mojom::DriveError::Type::kCantUploadStorageFull, base::FilePath("/foo"));
  mojom::DriveErrorPtr observed_error;
  EXPECT_CALL(observer, OnError(_)).WillOnce(CloneStruct(&observed_error));
  delegate_ptr_->OnError(error.Clone());
  delegate_ptr_.FlushForTesting();
  testing::Mock::VerifyAndClear(&observer);

  EXPECT_EQ(error, observed_error);
}

TEST_F(DriveFsHostTest, OnError_IgnoreUnknownErrorTypes) {
  ASSERT_NO_FATAL_FAILURE(DoMount());
  MockDriveFsHostObserver observer;
  ScopedObserver<DriveFsHost, DriveFsHostObserver> observer_scoper(&observer);
  observer_scoper.Add(host_.get());
  EXPECT_CALL(observer, OnError(_)).Times(0);
  delegate_ptr_->OnError(mojom::DriveError::New(
      static_cast<mojom::DriveError::Type>(
          static_cast<std::underlying_type_t<mojom::DriveError::Type>>(
              mojom::DriveError::Type::kMaxValue) +
          1),
      base::FilePath("/foo")));
  delegate_ptr_.FlushForTesting();
}

TEST_F(DriveFsHostTest, TeamDriveTracking) {
  ASSERT_NO_FATAL_FAILURE(DoMount());

  delegate_ptr_->OnTeamDrivesListReady({"a", "b"});
  delegate_ptr_.FlushForTesting();
  EXPECT_EQ(
      (std::set<std::string>{"a", "b"}),
      host_delegate_->GetDriveNotificationManager().team_drive_ids_for_test());

  delegate_ptr_->OnTeamDriveChanged(
      "c", mojom::DriveFsDelegate::CreateOrDelete::kCreated);
  delegate_ptr_.FlushForTesting();
  EXPECT_EQ(
      (std::set<std::string>{"a", "b", "c"}),
      host_delegate_->GetDriveNotificationManager().team_drive_ids_for_test());

  delegate_ptr_->OnTeamDriveChanged(
      "b", mojom::DriveFsDelegate::CreateOrDelete::kDeleted);
  delegate_ptr_.FlushForTesting();
  EXPECT_EQ(
      (std::set<std::string>{"a", "c"}),
      host_delegate_->GetDriveNotificationManager().team_drive_ids_for_test());
}

TEST_F(DriveFsHostTest, Invalidation) {
  ASSERT_NO_FATAL_FAILURE(DoMount());

  delegate_ptr_->OnTeamDrivesListReady({"a", "b"});
  delegate_ptr_.FlushForTesting();

  EXPECT_CALL(mock_drivefs_,
              FetchChangeLogImpl(std::vector<std::pair<int64_t, std::string>>{
                  {123, ""}, {456, "a"}}));

  for (auto& observer :
       host_delegate_->GetDriveNotificationManager().observers_for_test()) {
    observer.OnNotificationReceived({{"", 123}, {"a", 456}});
  }
  binding_.FlushForTesting();
}

TEST_F(DriveFsHostTest, InvalidateAll) {
  ASSERT_NO_FATAL_FAILURE(DoMount());

  delegate_ptr_->OnTeamDrivesListReady({"a", "b"});
  delegate_ptr_.FlushForTesting();

  EXPECT_CALL(mock_drivefs_, FetchAllChangeLogs());

  for (auto& observer :
       host_delegate_->GetDriveNotificationManager().observers_for_test()) {
    observer.OnNotificationTimerFired();
  }
  binding_.FlushForTesting();
}

TEST_F(DriveFsHostTest, RemoveDriveNotificationObserver) {
  ASSERT_NO_FATAL_FAILURE(DoMount());

  delegate_ptr_->OnTeamDrivesListReady({"a", "b"});
  delegate_ptr_.FlushForTesting();
  EXPECT_TRUE(host_delegate_->GetDriveNotificationManager()
                  .observers_for_test()
                  .might_have_observers());

  host_.reset();

  EXPECT_FALSE(host_delegate_->GetDriveNotificationManager()
                   .observers_for_test()
                   .might_have_observers());
}

TEST_F(DriveFsHostTest, Remount_Cached) {
  ASSERT_NO_FATAL_FAILURE(DoMount());

  EXPECT_CALL(mock_identity_manager_,
              GetAccessToken("test@example.com", _, "drivefs"))
      .WillOnce(testing::Return(
          std::make_pair("auth token", GoogleServiceAuthError::NONE)));

  ExpectAccessToken(mojom::AccessTokenStatus::kSuccess, "auth token");

  base::Optional<base::TimeDelta> delay = base::TimeDelta::FromSeconds(5);
  EXPECT_CALL(*host_delegate_, OnUnmounted(delay));
  SendOnUnmounted(delay);
  base::RunLoop().RunUntilIdle();
  ASSERT_NO_FATAL_FAILURE(DoUnmount());

  // Second mount attempt should reuse already available token.
  ASSERT_NO_FATAL_FAILURE(DoMount());
  EXPECT_EQ("auth token", init_access_token_.value_or(""));
}

TEST_F(DriveFsHostTest, Remount_CachedOnceOnly) {
  ASSERT_NO_FATAL_FAILURE(DoMount());

  EXPECT_CALL(mock_identity_manager_,
              GetAccessToken("test@example.com", _, "drivefs"))
      .WillOnce(testing::Return(
          std::make_pair("auth token", GoogleServiceAuthError::NONE)))
      .WillOnce(testing::Return(
          std::make_pair("auth token 2", GoogleServiceAuthError::NONE)));

  ExpectAccessToken(mojom::AccessTokenStatus::kSuccess, "auth token");

  base::Optional<base::TimeDelta> delay = base::TimeDelta::FromSeconds(5);
  EXPECT_CALL(*host_delegate_, OnUnmounted(delay));
  SendOnUnmounted(delay);
  base::RunLoop().RunUntilIdle();
  ASSERT_NO_FATAL_FAILURE(DoUnmount());

  // Second mount attempt should reuse already available token.
  ASSERT_NO_FATAL_FAILURE(DoMount());
  EXPECT_EQ("auth token", init_access_token_.value_or(""));

  // But if it asks for token it goes straight to identity.
  ExpectAccessToken(mojom::AccessTokenStatus::kSuccess, "auth token 2");
}

TEST_F(DriveFsHostTest, Remount_CacheExpired) {
  ASSERT_NO_FATAL_FAILURE(DoMount());

  EXPECT_CALL(mock_identity_manager_,
              GetAccessToken("test@example.com", _, "drivefs"))
      .WillOnce(testing::Return(
          std::make_pair("auth token", GoogleServiceAuthError::NONE)))
      .WillOnce(testing::Return(
          std::make_pair("auth token 2", GoogleServiceAuthError::NONE)));

  ExpectAccessToken(mojom::AccessTokenStatus::kSuccess, "auth token");

  base::Optional<base::TimeDelta> delay = base::TimeDelta::FromSeconds(5);
  EXPECT_CALL(*host_delegate_, OnUnmounted(delay));
  SendOnUnmounted(delay);
  base::RunLoop().RunUntilIdle();
  ASSERT_NO_FATAL_FAILURE(DoUnmount());

  clock_.Advance(base::TimeDelta::FromHours(2));

  // As the token expired second mount should go to identity.
  ASSERT_NO_FATAL_FAILURE(DoMount());
  ExpectAccessToken(mojom::AccessTokenStatus::kSuccess, "auth token 2");
}

TEST_F(DriveFsHostTest, Remount_RequestInflight) {
  ASSERT_NO_FATAL_FAILURE(DoMount());
  mock_identity_manager_.set_pause_requests(true);

  delegate_ptr_->GetAccessToken(
      "client ID", "app ID", {"scope1", "scope2"},
      base::BindLambdaForTesting([&](mojom::AccessTokenStatus status,
                                     const std::string& token) { FAIL(); }));

  base::Optional<base::TimeDelta> delay = base::TimeDelta::FromSeconds(5);
  EXPECT_CALL(*host_delegate_, OnUnmounted(delay));
  SendOnUnmounted(delay);
  base::RunLoop().RunUntilIdle();
  ASSERT_NO_FATAL_FAILURE(DoUnmount());

  // Now the response is ready.
  ASSERT_EQ(1u, mock_identity_manager_.callbacks().size());
  std::move(mock_identity_manager_.callbacks().front())
      .Run("auth token", clock_.Now() + base::TimeDelta::FromHours(1),
           GoogleServiceAuthError(GoogleServiceAuthError::NONE));
  mock_identity_manager_.bindings_->FlushForTesting();

  // Second mount will reuse previous token.
  ASSERT_NO_FATAL_FAILURE(DoMount());
  EXPECT_EQ("auth token", init_access_token_.value_or(""));
}

TEST_F(DriveFsHostTest, Remount_RequestInflightCompleteAfterMount) {
  ASSERT_NO_FATAL_FAILURE(DoMount());
  mock_identity_manager_.set_pause_requests(true);

  delegate_ptr_->GetAccessToken(
      "client ID", "app ID", {"scope1", "scope2"},
      base::BindLambdaForTesting([&](mojom::AccessTokenStatus status,
                                     const std::string& token) { FAIL(); }));

  base::Optional<base::TimeDelta> delay = base::TimeDelta::FromSeconds(5);
  EXPECT_CALL(*host_delegate_, OnUnmounted(delay));
  SendOnUnmounted(delay);
  base::RunLoop().RunUntilIdle();
  ASSERT_NO_FATAL_FAILURE(DoUnmount());

  // Second mount will reuse previous token.
  ASSERT_NO_FATAL_FAILURE(DoMount());
  EXPECT_FALSE(init_access_token_);

  // Now the response is ready.
  ASSERT_EQ(1u, mock_identity_manager_.callbacks().size());
  std::move(mock_identity_manager_.callbacks().front())
      .Run("auth token", clock_.Now() + base::TimeDelta::FromHours(1),
           GoogleServiceAuthError(GoogleServiceAuthError::NONE));
  mock_identity_manager_.bindings_->FlushForTesting();

  // A new request will reuse the cached token.
  ExpectAccessToken(mojom::AccessTokenStatus::kSuccess, "auth token");
}

ACTION_P(PopulateSearch, count) {
  if (count < 0)
    return;
  std::vector<mojom::QueryItemPtr> items;
  for (int i = 0; i < count; ++i) {
    items.emplace_back(mojom::QueryItem::New());
    items.back()->metadata = mojom::FileMetadata::New();
    items.back()->metadata->capabilities = mojom::Capabilities::New();
  }
  *arg0 = std::move(items);
}

MATCHER_P5(MatchQuery, source, text, title, shared, offline, "") {
  if (arg.query_source != source)
    return false;
  if (text != nullptr) {
    if (!arg.text_content || *arg.text_content != base::StringPiece(text))
      return false;
  } else {
    if (arg.text_content)
      return false;
  }
  if (title != nullptr) {
    if (!arg.title || *arg.title != base::StringPiece(title))
      return false;
  } else {
    if (arg.title)
      return false;
  }
  return arg.shared_with_me == shared && arg.available_offline == offline;
};

TEST_F(DriveFsHostTest, Search) {
  ASSERT_NO_FATAL_FAILURE(DoMount());

  EXPECT_CALL(mock_drivefs_, OnStartSearchQuery(_));
  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_))
      .WillOnce(testing::DoAll(
          PopulateSearch(3), testing::Return(drive::FileError::FILE_ERROR_OK)));

  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kLocalOnly;

  bool called = false;
  mojom::QueryParameters::QuerySource source = host_->PerformSearch(
      std::move(params),
      base::BindLambdaForTesting(
          [&called](drive::FileError err,
                    base::Optional<std::vector<mojom::QueryItemPtr>> items) {
            called = true;
            EXPECT_EQ(drive::FileError::FILE_ERROR_OK, err);
            EXPECT_EQ(3u, items->size());
          }));
  EXPECT_EQ(mojom::QueryParameters::QuerySource::kLocalOnly, source);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
}

TEST_F(DriveFsHostTest, Search_Fail) {
  ASSERT_NO_FATAL_FAILURE(DoMount());

  EXPECT_CALL(mock_drivefs_, OnStartSearchQuery(_));
  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_))
      .WillOnce(testing::Return(drive::FileError::FILE_ERROR_ACCESS_DENIED));

  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kCloudOnly;

  bool called = false;
  mojom::QueryParameters::QuerySource source = host_->PerformSearch(
      std::move(params),
      base::BindLambdaForTesting(
          [&called](drive::FileError err,
                    base::Optional<std::vector<mojom::QueryItemPtr>> items) {
            called = true;
            EXPECT_EQ(drive::FileError::FILE_ERROR_ACCESS_DENIED, err);
          }));
  EXPECT_EQ(mojom::QueryParameters::QuerySource::kCloudOnly, source);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
}

TEST_F(DriveFsHostTest, Search_OnlineToOffline) {
  ASSERT_NO_FATAL_FAILURE(DoMount());

  network_.SetConnectionType(
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE);

  EXPECT_CALL(mock_drivefs_, OnStartSearchQuery(_));
  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_))
      .WillOnce(testing::DoAll(
          PopulateSearch(3), testing::Return(drive::FileError::FILE_ERROR_OK)));

  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kCloudOnly;

  bool called = false;
  mojom::QueryParameters::QuerySource source = host_->PerformSearch(
      std::move(params),
      base::BindLambdaForTesting(
          [&called](drive::FileError err,
                    base::Optional<std::vector<mojom::QueryItemPtr>> items) {
            called = true;
            EXPECT_EQ(drive::FileError::FILE_ERROR_OK, err);
            EXPECT_EQ(3u, items->size());
          }));
  EXPECT_EQ(mojom::QueryParameters::QuerySource::kLocalOnly, source);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
}

TEST_F(DriveFsHostTest, Search_OnlineToOfflineFallback) {
  ASSERT_NO_FATAL_FAILURE(DoMount());

  EXPECT_CALL(mock_drivefs_,
              OnStartSearchQuery(
                  MatchQuery(mojom::QueryParameters::QuerySource::kCloudOnly,
                             "foobar", nullptr, false, false)));
  EXPECT_CALL(mock_drivefs_,
              OnStartSearchQuery(
                  MatchQuery(mojom::QueryParameters::QuerySource::kLocalOnly,
                             nullptr, "foobar", false, false)));

  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_))
      .WillOnce(testing::Return(drive::FileError::FILE_ERROR_NO_CONNECTION))
      .WillOnce(testing::DoAll(
          PopulateSearch(3), testing::Return(drive::FileError::FILE_ERROR_OK)));

  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kCloudOnly;
  params->text_content = "foobar";

  bool called = false;
  mojom::QueryParameters::QuerySource source = host_->PerformSearch(
      std::move(params),
      base::BindLambdaForTesting(
          [&called](drive::FileError err,
                    base::Optional<std::vector<mojom::QueryItemPtr>> items) {
            called = true;
            EXPECT_EQ(drive::FileError::FILE_ERROR_OK, err);
            EXPECT_EQ(3u, items->size());
          }));
  EXPECT_EQ(mojom::QueryParameters::QuerySource::kCloudOnly, source);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
}

TEST_F(DriveFsHostTest, Search_SharedWithMeCaching) {
  ASSERT_NO_FATAL_FAILURE(DoMount());

  EXPECT_CALL(mock_drivefs_,
              OnStartSearchQuery(
                  MatchQuery(mojom::QueryParameters::QuerySource::kCloudOnly,
                             nullptr, nullptr, true, false)))
      .Times(2);
  EXPECT_CALL(mock_drivefs_,
              OnStartSearchQuery(
                  MatchQuery(mojom::QueryParameters::QuerySource::kLocalOnly,
                             nullptr, nullptr, true, false)))
      .Times(1);

  EXPECT_CALL(mock_drivefs_, OnGetNextPage(_))
      .WillOnce(testing::DoAll(
          PopulateSearch(3), testing::Return(drive::FileError::FILE_ERROR_OK)))
      .WillOnce(testing::DoAll(
          PopulateSearch(3), testing::Return(drive::FileError::FILE_ERROR_OK)))
      .WillOnce(testing::DoAll(
          PopulateSearch(3), testing::Return(drive::FileError::FILE_ERROR_OK)));

  mojom::QueryParametersPtr params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kCloudOnly;
  params->shared_with_me = true;

  bool called = false;
  mojom::QueryParameters::QuerySource source = host_->PerformSearch(
      std::move(params),
      base::BindLambdaForTesting(
          [&called](drive::FileError err,
                    base::Optional<std::vector<mojom::QueryItemPtr>> items) {
            called = true;
            EXPECT_EQ(drive::FileError::FILE_ERROR_OK, err);
            EXPECT_EQ(3u, items->size());
          }));
  EXPECT_EQ(mojom::QueryParameters::QuerySource::kCloudOnly, source);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);

  params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kCloudOnly;
  params->shared_with_me = true;

  called = false;
  source = host_->PerformSearch(
      std::move(params),
      base::BindLambdaForTesting(
          [&called](drive::FileError err,
                    base::Optional<std::vector<mojom::QueryItemPtr>> items) {
            called = true;
            EXPECT_EQ(drive::FileError::FILE_ERROR_OK, err);
            EXPECT_EQ(3u, items->size());
          }));
  EXPECT_EQ(mojom::QueryParameters::QuerySource::kLocalOnly, source);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);

  // Time has passed...
  clock_.Advance(base::TimeDelta::FromHours(1));

  params = mojom::QueryParameters::New();
  params->query_source = mojom::QueryParameters::QuerySource::kCloudOnly;
  params->shared_with_me = true;

  called = false;
  source = host_->PerformSearch(
      std::move(params),
      base::BindLambdaForTesting(
          [&called](drive::FileError err,
                    base::Optional<std::vector<mojom::QueryItemPtr>> items) {
            called = true;
            EXPECT_EQ(drive::FileError::FILE_ERROR_OK, err);
            EXPECT_EQ(3u, items->size());
          }));
  EXPECT_EQ(mojom::QueryParameters::QuerySource::kCloudOnly, source);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
}

}  // namespace
}  // namespace drivefs
