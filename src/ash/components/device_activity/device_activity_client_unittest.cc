// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/device_activity/device_activity_client.h"

#include "ash/components/device_activity/daily_use_case_impl.h"
#include "ash/components/device_activity/device_activity_controller.h"
#include "ash/components/device_activity/fresnel_pref_names.h"
#include "ash/components/device_activity/fresnel_service.pb.h"
#include "ash/components/device_activity/monthly_use_case_impl.h"
#include "ash/constants/ash_features.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "chromeos/network/network_state_test_helper.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "components/prefs/testing_pref_service.h"
#include "components/version_info/channel.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "third_party/private_membership/src/internal/testing/regression_test_data/regression_test_data.pb.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

namespace ash {
namespace device_activity {

namespace psm_rlwe = private_membership::rlwe;

namespace {

// Holds data used to create deterministic PSM network request/response protos.
struct PsmTestData {
  // Holds the response bodies used to test the case where the plaintext id is
  // a member of the PSM dataset.
  psm_rlwe::PrivateMembershipRlweClientRegressionTestData::TestCase
      member_test_case;

  // Holds the response bodies used to test the case where the plaintext id is
  // not a member of the PSM dataset.
  psm_rlwe::PrivateMembershipRlweClientRegressionTestData::TestCase
      nonmember_test_case;
};

PsmTestData* GetPsmTestData() {
  static base::NoDestructor<PsmTestData> data;
  return data.get();
}

// TODO(https://crbug.com/1272922): Move shared configuration constants to
// separate file.
//
// URLs for the different network requests being performed.
const char kTestFresnelBaseUrl[] = "https://dummy.googleapis.com";
const char kPsmImportRequestEndpoint[] = "/v1/fresnel/psmRlweImport";
const char kPsmOprfRequestEndpoint[] = "/v1/fresnel/psmRlweOprf";
const char kPsmQueryRequestEndpoint[] = "/v1/fresnel/psmRlweQuery";

// Create fake secrets used by the |DeviceActivityClient|.
constexpr char kFakePsmDeviceActiveSecret[] = "FAKE_PSM_DEVICE_ACTIVE_SECRET";
constexpr char kFakeFresnelApiKey[] = "FAKE_FRESNEL_API_KEY";

const version_info::Channel kFakeChromeOSChannel =
    version_info::Channel::STABLE;

// Number of test cases exist in cros_test_data.binarypb file, which is part of
// private_membership third_party library.
const int kNumberOfPsmTestCases = 10;

// PrivateSetMembership regression tests maximum file size which is 4MB.
const size_t kMaxFileSizeInBytes = 4 * (1 << 20);

std::string GetFresnelTestEndpoint(const std::string& endpoint) {
  return kTestFresnelBaseUrl + endpoint;
}

bool ParseProtoFromFile(const base::FilePath& file_path,
                        google::protobuf::MessageLite* out_proto) {
  if (!out_proto)
    return false;

  std::string file_content;
  if (!base::ReadFileToStringWithMaxSize(file_path, &file_content,
                                         kMaxFileSizeInBytes)) {
    return false;
  }
  return out_proto->ParseFromString(file_content);
}

base::TimeDelta TimeUntilNextUTCMidnight() {
  const auto now = base::Time::Now();
  return (now.UTCMidnight() + base::Hours(base::Time::kHoursPerDay) - now);
}

base::TimeDelta TimeUntilNewUTCMonth() {
  const auto current_ts = base::Time::Now();

  base::Time::Exploded exploded_current_ts;
  current_ts.UTCExplode(&exploded_current_ts);

  // Exploded structure uses 1-based month (values 1 = January, etc.)
  // Increment current ts to be the new month/year.
  if (exploded_current_ts.month == 12) {
    exploded_current_ts.month = 1;
    exploded_current_ts.year += 1;
  } else {
    exploded_current_ts.month += 1;
  }

  // New timestamp should reflect first day of new month.
  exploded_current_ts.day_of_month = 1;

  base::Time new_ts;
  EXPECT_TRUE(base::Time::FromUTCExploded(exploded_current_ts, &new_ts));

  return new_ts - current_ts;
}

}  // namespace

class FakePsmDelegate : public PsmDelegate {
 public:
  FakePsmDelegate(const std::string& ec_cipher_key,
                  const std::string& seed,
                  const std::vector<psm_rlwe::RlwePlaintextId>& plaintext_ids)
      : ec_cipher_key_(ec_cipher_key),
        seed_(seed),
        plaintext_ids_(plaintext_ids) {}
  FakePsmDelegate(const FakePsmDelegate&) = delete;
  FakePsmDelegate& operator=(const FakePsmDelegate&) = delete;
  ~FakePsmDelegate() override = default;

  // PsmDelegate:
  rlwe::StatusOr<
      std::unique_ptr<private_membership::rlwe::PrivateMembershipRlweClient>>
  CreatePsmClient(private_membership::rlwe::RlweUseCase use_case,
                  const std::vector<private_membership::rlwe::RlwePlaintextId>&
                      plaintext_ids) override {
    return psm_rlwe::PrivateMembershipRlweClient::CreateForTesting(
        use_case, plaintext_ids_, ec_cipher_key_, seed_);
  }

 private:
  // Used by the PSM client to generate deterministic request/response protos.
  std::string ec_cipher_key_;
  std::string seed_;
  std::vector<psm_rlwe::RlwePlaintextId> plaintext_ids_;
};

class FakeDailyUseCaseImpl : public DailyUseCaseImpl {
 public:
  FakeDailyUseCaseImpl(const std::string& psm_device_active_secret,
                       version_info::Channel chromeos_channel,
                       PrefService* local_state)
      : DailyUseCaseImpl(psm_device_active_secret,
                         chromeos_channel,
                         local_state) {}
  FakeDailyUseCaseImpl(const FakeDailyUseCaseImpl&) = delete;
  FakeDailyUseCaseImpl& operator=(const FakeDailyUseCaseImpl&) = delete;
  ~FakeDailyUseCaseImpl() override = default;
};

class FakeMonthlyUseCaseImpl : public MonthlyUseCaseImpl {
 public:
  FakeMonthlyUseCaseImpl(const std::string& psm_device_active_secret,
                         version_info::Channel chromeos_channel,
                         PrefService* local_state)
      : MonthlyUseCaseImpl(psm_device_active_secret,
                           chromeos_channel,
                           local_state) {}
  FakeMonthlyUseCaseImpl(const FakeMonthlyUseCaseImpl&) = delete;
  FakeMonthlyUseCaseImpl& operator=(const FakeMonthlyUseCaseImpl&) = delete;
  ~FakeMonthlyUseCaseImpl() override = default;
};

// TODO(crbug/1317652): Refactor checking if current use case local pref is
// unset. We may also want to abstract the psm network responses for the unit
// tests.
class DeviceActivityClientTest : public testing::Test {
 public:
  DeviceActivityClientTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    // Remote env. runs unit tests assuming base::Time::Now() is epoch.
    // Forward current time to 2022-01-01 00:00:00.
    base::Time new_current_ts;
    EXPECT_TRUE(
        base::Time::FromUTCString("2022-01-01 00:00:00", &new_current_ts));
    task_environment_.FastForwardBy(new_current_ts - base::Time::Now());
    task_environment_.RunUntilIdle();
  }
  DeviceActivityClientTest(const DeviceActivityClientTest&) = delete;
  DeviceActivityClientTest& operator=(const DeviceActivityClientTest&) = delete;
  ~DeviceActivityClientTest() override = default;

 protected:
  static void SetUpTestSuite() {
    // Initialize |psm_test_case_| which is used to generate deterministic psm
    // protos.
    CreatePsmTestCase();
  }

  static void CreatePsmTestCase() {
    base::FilePath src_root_dir;
    ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &src_root_dir));
    const base::FilePath kPsmTestDataPath =
        src_root_dir.AppendASCII("third_party")
            .AppendASCII("private_membership")
            .AppendASCII("src")
            .AppendASCII("internal")
            .AppendASCII("testing")
            .AppendASCII("regression_test_data")
            .AppendASCII("test_data.binarypb");
    ASSERT_TRUE(base::PathExists(kPsmTestDataPath));
    psm_rlwe::PrivateMembershipRlweClientRegressionTestData test_data;
    ASSERT_TRUE(ParseProtoFromFile(kPsmTestDataPath, &test_data));

    // Note that the test cases can change since it's read from the binarypb.
    // This can cause unexpected failures for the unit tests below.
    // As a safety precaution, check whether the number of tests change.
    ASSERT_EQ(test_data.test_cases_size(), kNumberOfPsmTestCases);

    // Sets |psm_test_case_| to have one of the fake PSM request/response
    // protos.
    //
    // Test case 0 contains a response where check membership returns true.
    // Test case 5 contains a response where check membership returns false.
    GetPsmTestData()->member_test_case = test_data.test_cases(0);
    GetPsmTestData()->nonmember_test_case = test_data.test_cases(5);
  }

  std::vector<psm_rlwe::RlwePlaintextId> GetPlaintextIds(
      const psm_rlwe::PrivateMembershipRlweClientRegressionTestData::TestCase&
          test_case) {
    // Return well formed plaintext ids used in faking PSM network requests.
    return {test_case.plaintext_id()};
  }

  // Initialize well formed OPRF response body used to deterministically fake
  // PSM network responses.
  std::string GetFresnelOprfResponse(
      const psm_rlwe::PrivateMembershipRlweClientRegressionTestData::TestCase&
          test_case) {
    FresnelPsmRlweOprfResponse psm_oprf_response;
    *psm_oprf_response.mutable_rlwe_oprf_response() = test_case.oprf_response();
    return psm_oprf_response.SerializeAsString();
  }

  // Initialize well formed Query response body used to deterministically fake
  // PSM network responses.
  std::string GetFresnelQueryResponse(
      const psm_rlwe::PrivateMembershipRlweClientRegressionTestData::TestCase&
          test_case) {
    FresnelPsmRlweQueryResponse psm_query_response;
    *psm_query_response.mutable_rlwe_query_response() =
        test_case.query_response();
    return psm_query_response.SerializeAsString();
  }

  // testing::Test:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kDeviceActiveClientMonthlyCheckIn,
                              features::kDeviceActiveClientDailyCheckMembership,
                              features::
                                  kDeviceActiveClientMonthlyCheckMembership},
        /*disabled_features*/ {});

    // Initialize pointer to our fake |PsmTestData| object.
    psm_test_data_ = GetPsmTestData();

    network_state_test_helper_ = std::make_unique<NetworkStateTestHelper>(
        /*use_default_devices_and_services=*/false);
    CreateWifiNetworkConfig();

    // Initialize |local_state_| prefs used by device_activity_client class.
    DeviceActivityController::RegisterPrefs(local_state_.registry());
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    chromeos::system::StatisticsProvider::SetTestProvider(
        &statistics_provider_);

    // Create vector of device active use cases, which device activity client
    // should maintain ownership of.
    std::vector<std::unique_ptr<DeviceActiveUseCase>> use_cases;
    use_cases.push_back(std::make_unique<FakeDailyUseCaseImpl>(
        kFakePsmDeviceActiveSecret, kFakeChromeOSChannel, &local_state_));
    use_cases.push_back(std::make_unique<FakeMonthlyUseCaseImpl>(
        kFakePsmDeviceActiveSecret, kFakeChromeOSChannel, &local_state_));

    device_activity_client_ = std::make_unique<DeviceActivityClient>(
        network_state_test_helper_->network_state_handler(),
        test_shared_loader_factory_,
        // |FakePsmDelegate| can use any test case parameters.
        std::make_unique<FakePsmDelegate>(
            psm_test_data_->nonmember_test_case.ec_cipher_key(),
            psm_test_data_->nonmember_test_case.seed(),
            GetPlaintextIds(psm_test_data_->nonmember_test_case)),
        std::make_unique<base::MockRepeatingTimer>(), kTestFresnelBaseUrl,
        kFakeFresnelApiKey, std::move(use_cases));
  }

  void TearDown() override {}

  void SimulateLocalStateOnPowerwash() {
    // Simulate powerwashing device by removing the local state prefs.
    local_state_.RemoveUserPref(
        prefs::kDeviceActiveLastKnownDailyPingTimestamp);
    local_state_.RemoveUserPref(
        prefs::kDeviceActiveLastKnownMonthlyPingTimestamp);
  }

  void CreateWifiNetworkConfig() {
    ASSERT_TRUE(wifi_network_service_path_.empty());

    std::stringstream ss;
    ss << "{"
       << "  \"GUID\": \""
       << "wifi_guid"
       << "\","
       << "  \"Type\": \"" << shill::kTypeWifi << "\","
       << "  \"State\": \"" << shill::kStateOffline << "\""
       << "}";

    wifi_network_service_path_ =
        network_state_test_helper_->ConfigureService(ss.str());
  }

  // |network_state| is a shill network state, e.g. "shill::kStateIdle".
  void SetWifiNetworkState(std::string network_state) {
    network_state_test_helper_->SetServiceProperty(wifi_network_service_path_,
                                                   shill::kStateProperty,
                                                   base::Value(network_state));
    task_environment_.RunUntilIdle();
  }

  // Used in tests, after |device_activity_client_| is generated.
  // Triggers the repeating timer in the client code.
  void FireTimer() {
    base::MockRepeatingTimer* mock_timer =
        static_cast<base::MockRepeatingTimer*>(
            device_activity_client_->GetReportTimer());
    if (mock_timer->IsRunning())
      mock_timer->Fire();
  }

  base::test::TaskEnvironment task_environment_;

  // The underlying |psm_test_data_| object will outlive this testing class.
  PsmTestData* psm_test_data_ = nullptr;

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<NetworkStateTestHelper> network_state_test_helper_;
  TestingPrefServiceSimple local_state_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<DeviceActivityClient> device_activity_client_;
  std::string wifi_network_service_path_;
  base::HistogramTester histogram_tester_;
  chromeos::system::FakeStatisticsProvider statistics_provider_;
};

TEST_F(DeviceActivityClientTest, DefaultStatesAreInitializedProperly) {
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);

  for (auto* use_case : device_activity_client_->GetUseCases()) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    EXPECT_EQ(use_case->GetLastKnownPingTimestamp(), base::Time::UnixEpoch());
  }

  EXPECT_TRUE(device_activity_client_->GetReportTimer()->IsRunning());
}

TEST_F(DeviceActivityClientTest, NetworkRequestsUseFakeApiKey) {
  // When network comes online, the device performs a check membership
  // network request.
  SetWifiNetworkState(shill::kStateOnline);

  network::TestURLLoaderFactory::PendingRequest* request =
      test_url_loader_factory_.GetPendingRequest(0);
  task_environment_.RunUntilIdle();

  std::string api_key_header_value;
  request->request.headers.GetHeader("X-Goog-Api-Key", &api_key_header_value);

  EXPECT_EQ(api_key_header_value, kFakeFresnelApiKey);
}

// Fire timer to run |TransitionOutOfIdle|. Network is currently disconnected
// so the client is expected to go back to |kIdle| state.
TEST_F(DeviceActivityClientTest,
       FireTimerWithoutNetworkKeepsClientinIdleState) {
  SetWifiNetworkState(shill::kStateOffline);
  FireTimer();

  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
}

TEST_F(DeviceActivityClientTest, NetworkReconnectsAfterSuccessfulCheckIn) {
  // Device active reporting starts check membership on network connect.
  SetWifiNetworkState(shill::kStateOnline);

  // |nonmember_test_case| is used to return psm response bodies for
  // the OPRF, and Query requests. The query request returns nonmember status.
  const psm_rlwe::PrivateMembershipRlweClientRegressionTestData::TestCase&
      nonmember_test_case = psm_test_data_->nonmember_test_case;

  for (auto* use_case : device_activity_client_->GetUseCases()) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    EXPECT_EQ(device_activity_client_->GetState(),
              DeviceActivityClient::State::kCheckingMembershipOprf);

    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFresnelTestEndpoint(kPsmOprfRequestEndpoint),
        GetFresnelOprfResponse(nonmember_test_case), net::HTTP_OK);
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFresnelTestEndpoint(kPsmQueryRequestEndpoint),
        GetFresnelQueryResponse(nonmember_test_case), net::HTTP_OK);
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFresnelTestEndpoint(kPsmImportRequestEndpoint), std::string(),
        net::HTTP_OK);
    task_environment_.RunUntilIdle();
  }

  // Reconnecting network connection triggers |TransitionOutOfIdle|.
  SetWifiNetworkState(shill::kStateOffline);
  SetWifiNetworkState(shill::kStateOnline);

  // Check that no additional network requests are pending since all use cases
  // have already been imported.
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
}

TEST_F(DeviceActivityClientTest,
       CheckMembershipOnLocalStateUnsetAndPingRequired) {
  // Device active reporting starts check membership on network connect.
  SetWifiNetworkState(shill::kStateOnline);

  // |nonmember_test_case| is used to return psm response bodies for
  // the OPRF, and Query requests. The query request returns nonmember status.
  const psm_rlwe::PrivateMembershipRlweClientRegressionTestData::TestCase&
      nonmember_test_case = psm_test_data_->nonmember_test_case;

  for (auto* use_case : device_activity_client_->GetUseCases()) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    // On first ever ping, we begin the check membership protocol
    // since the local state pref for that use case is by default unix
    // epoch.
    EXPECT_EQ(use_case->GetLastKnownPingTimestamp(), base::Time::UnixEpoch());
    EXPECT_EQ(device_activity_client_->GetState(),
              DeviceActivityClient::State::kCheckingMembershipOprf);

    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFresnelTestEndpoint(kPsmOprfRequestEndpoint),
        GetFresnelOprfResponse(nonmember_test_case), net::HTTP_OK);
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFresnelTestEndpoint(kPsmQueryRequestEndpoint),
        GetFresnelQueryResponse(nonmember_test_case), net::HTTP_OK);
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFresnelTestEndpoint(kPsmImportRequestEndpoint), std::string(),
        net::HTTP_OK);
    task_environment_.RunUntilIdle();

    EXPECT_NE(use_case->GetLastKnownPingTimestamp(), base::Time::UnixEpoch());
  }

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
}

TEST_F(DeviceActivityClientTest, CheckInOnLocalStateSetAndPingRequired) {
  // Set the use cases last ping timestamps to a previous month.
  // This date must be ahead of unix epoch, since that is the default value
  // of the local state time pref.
  // The current time in the unit tests is 10 years after unix epoch.
  base::Time expected;
  ASSERT_TRUE(base::Time::FromUTCString("2000-01-01 00:00:00", &expected));
  for (auto* use_case : device_activity_client_->GetUseCases()) {
    use_case->SetLastKnownPingTimestamp(expected);
  }

  // Device active reporting starts check in on network connect.
  SetWifiNetworkState(shill::kStateOnline);

  for (auto* use_case : device_activity_client_->GetUseCases()) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    EXPECT_NE(use_case->GetLastKnownPingTimestamp(), base::Time::UnixEpoch());
    EXPECT_EQ(device_activity_client_->GetState(),
              DeviceActivityClient::State::kCheckingIn);

    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFresnelTestEndpoint(kPsmImportRequestEndpoint), std::string(),
        net::HTTP_OK);
    task_environment_.RunUntilIdle();

    // base::Time::Now() is updated in |DeviceActivityClientTest| constructor.
    EXPECT_GE(use_case->GetLastKnownPingTimestamp(), expected);
  }

  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
}

TEST_F(DeviceActivityClientTest, TransitionClientToIdleOnInvalidOprfResponse) {
  // Device active reporting starts check membership on network connect.
  SetWifiNetworkState(shill::kStateOnline);

  for (auto* use_case : device_activity_client_->GetUseCases()) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    EXPECT_EQ(device_activity_client_->GetState(),
              DeviceActivityClient::State::kCheckingMembershipOprf);

    // Return an invalid Fresnel OPRF response.
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFresnelTestEndpoint(kPsmOprfRequestEndpoint),
        /*fresnel_oprf_response*/ std::string(), net::HTTP_OK);
    task_environment_.RunUntilIdle();
  }

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
}

TEST_F(DeviceActivityClientTest, TransitionClientToIdleOnInvalidQueryResponse) {
  // Device active reporting starts check membership on network connect.
  SetWifiNetworkState(shill::kStateOnline);

  // |nonmember_test_case| is used to return psm response bodies for
  // the OPRF, and Query requests. The query request returns nonmember status.
  const psm_rlwe::PrivateMembershipRlweClientRegressionTestData::TestCase&
      nonmember_test_case = psm_test_data_->nonmember_test_case;

  for (auto* use_case : device_activity_client_->GetUseCases()) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    EXPECT_EQ(device_activity_client_->GetState(),
              DeviceActivityClient::State::kCheckingMembershipOprf);

    // Return a valid OPRF response.
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFresnelTestEndpoint(kPsmOprfRequestEndpoint),
        GetFresnelOprfResponse(nonmember_test_case), net::HTTP_OK);

    // Return an invalid Query response.
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFresnelTestEndpoint(kPsmQueryRequestEndpoint), std::string(),
        net::HTTP_OK);
    task_environment_.RunUntilIdle();
  }

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
}

TEST_F(DeviceActivityClientTest, DailyCheckInFailsButMonthlyCheckInSucceeds) {
  // Device active reporting starts check membership on network connect.
  SetWifiNetworkState(shill::kStateOnline);

  // |nonmember_test_case| is used to return psm response bodies for
  // the OPRF, and Query requests. The query request returns nonmember status.
  const psm_rlwe::PrivateMembershipRlweClientRegressionTestData::TestCase&
      nonmember_test_case = psm_test_data_->nonmember_test_case;

  for (auto* use_case : device_activity_client_->GetUseCases()) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    // On first ever ping, we begin the check membership protocol
    // since the local state pref for that use case is by default unix
    // epoch.
    EXPECT_EQ(use_case->GetLastKnownPingTimestamp(), base::Time::UnixEpoch());
    EXPECT_EQ(device_activity_client_->GetState(),
              DeviceActivityClient::State::kCheckingMembershipOprf);

    if (use_case->GetPsmUseCase() ==
        psm_rlwe::RlweUseCase::CROS_FRESNEL_DAILY) {
      // Daily use case will terminate while failing to parse
      // this invalid OPRF response.
      test_url_loader_factory_.SimulateResponseForPendingRequest(
          GetFresnelTestEndpoint(kPsmOprfRequestEndpoint), std::string(),
          net::HTTP_OK);

      task_environment_.RunUntilIdle();

      // Failed to update the local state since the OPRF response was invalid.
      EXPECT_EQ(use_case->GetLastKnownPingTimestamp(), base::Time::UnixEpoch());
    } else if (use_case->GetPsmUseCase() ==
               psm_rlwe::RlweUseCase::CROS_FRESNEL_MONTHLY) {
      // Monthly use case will return valid psm network request responses.
      test_url_loader_factory_.SimulateResponseForPendingRequest(
          GetFresnelTestEndpoint(kPsmOprfRequestEndpoint),
          GetFresnelOprfResponse(nonmember_test_case), net::HTTP_OK);
      test_url_loader_factory_.SimulateResponseForPendingRequest(
          GetFresnelTestEndpoint(kPsmQueryRequestEndpoint),
          GetFresnelQueryResponse(nonmember_test_case), net::HTTP_OK);
      test_url_loader_factory_.SimulateResponseForPendingRequest(
          GetFresnelTestEndpoint(kPsmImportRequestEndpoint), std::string(),
          net::HTTP_OK);

      task_environment_.RunUntilIdle();

      // Successfully imported and updated the last ping timestamp to the
      // current mocked time for this test.
      EXPECT_EQ(use_case->GetLastKnownPingTimestamp(), base::Time::Now());
    } else {
      // Currently we only support daily, and monthly use cases.
      NOTREACHED() << "Invalid Use Case.";
    }
  }

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
}

TEST_F(DeviceActivityClientTest, MonthlyCheckInFailsButDailyCheckInSucceeds) {
  // Device active reporting starts check membership on network connect.
  SetWifiNetworkState(shill::kStateOnline);

  // |nonmember_test_case| is used to return psm response bodies for
  // the OPRF, and Query requests. The query request returns nonmember status.
  const psm_rlwe::PrivateMembershipRlweClientRegressionTestData::TestCase&
      nonmember_test_case = psm_test_data_->nonmember_test_case;

  for (auto* use_case : device_activity_client_->GetUseCases()) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    // On first ever ping, we begin the check membership protocol
    // since the local state pref for that use case is by default unix
    // epoch.
    EXPECT_EQ(use_case->GetLastKnownPingTimestamp(), base::Time::UnixEpoch());
    EXPECT_EQ(device_activity_client_->GetState(),
              DeviceActivityClient::State::kCheckingMembershipOprf);

    if (use_case->GetPsmUseCase() ==
        psm_rlwe::RlweUseCase::CROS_FRESNEL_DAILY) {
      // Daily use case will return valid psm network request responses.
      test_url_loader_factory_.SimulateResponseForPendingRequest(
          GetFresnelTestEndpoint(kPsmOprfRequestEndpoint),
          GetFresnelOprfResponse(nonmember_test_case), net::HTTP_OK);
      test_url_loader_factory_.SimulateResponseForPendingRequest(
          GetFresnelTestEndpoint(kPsmQueryRequestEndpoint),
          GetFresnelQueryResponse(nonmember_test_case), net::HTTP_OK);
      test_url_loader_factory_.SimulateResponseForPendingRequest(
          GetFresnelTestEndpoint(kPsmImportRequestEndpoint), std::string(),
          net::HTTP_OK);

      task_environment_.RunUntilIdle();

      // Successfully imported and updated the last ping timestamp to the
      // current mocked time for this test.
      EXPECT_EQ(use_case->GetLastKnownPingTimestamp(), base::Time::Now());
    } else if (use_case->GetPsmUseCase() ==
               psm_rlwe::RlweUseCase::CROS_FRESNEL_MONTHLY) {
      // Monthly use case will terminate while failing to parse
      // this invalid OPRF response.
      test_url_loader_factory_.SimulateResponseForPendingRequest(
          GetFresnelTestEndpoint(kPsmOprfRequestEndpoint), std::string(),
          net::HTTP_OK);

      task_environment_.RunUntilIdle();

      // Failed to update the local state since the OPRF response was invalid.
      EXPECT_EQ(use_case->GetLastKnownPingTimestamp(), base::Time::UnixEpoch());
    } else {
      // Currently we only support daily, and monthly use cases.
      NOTREACHED() << "Invalid Use Case.";
    }
  }

  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
}

TEST_F(DeviceActivityClientTest, CurrentTimeIsBeforeLocalStateTimeStamp) {
  // Update last ping timestamps to a time in the future.
  base::Time expected;
  ASSERT_TRUE(base::Time::FromUTCString("2100-01-01 00:00:00", &expected));
  for (auto* use_case : device_activity_client_->GetUseCases()) {
    use_case->SetLastKnownPingTimestamp(expected);
  }

  // Device active reporting is triggered by network connection.
  SetWifiNetworkState(shill::kStateOnline);

  // Device pings are not required since the last ping timestamps are in the
  // future. Client will stay in |kIdle| state.
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
}

TEST_F(DeviceActivityClientTest, StayIdleIfTimerFiresWithoutNetworkConnected) {
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);

  SetWifiNetworkState(shill::kStateOffline);
  FireTimer();

  // Verify that no network requests were sent.
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
}

TEST_F(DeviceActivityClientTest, CheckInIfCheckMembershipReturnsFalse) {
  // Device active reporting starts check membership on network connect.
  SetWifiNetworkState(shill::kStateOnline);

  // |nonmember_test_case| is used to return psm response bodies for
  // the OPRF, and Query requests. The query request returns nonmember status.
  const psm_rlwe::PrivateMembershipRlweClientRegressionTestData::TestCase&
      nonmember_test_case = psm_test_data_->nonmember_test_case;

  for (auto* use_case : device_activity_client_->GetUseCases()) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    EXPECT_EQ(device_activity_client_->GetState(),
              DeviceActivityClient::State::kCheckingMembershipOprf);
    base::Time prev_time = use_case->GetLastKnownPingTimestamp();

    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFresnelTestEndpoint(kPsmOprfRequestEndpoint),
        GetFresnelOprfResponse(nonmember_test_case), net::HTTP_OK);
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFresnelTestEndpoint(kPsmQueryRequestEndpoint),
        GetFresnelQueryResponse(nonmember_test_case), net::HTTP_OK);
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFresnelTestEndpoint(kPsmImportRequestEndpoint), std::string(),
        net::HTTP_OK);
    task_environment_.RunUntilIdle();

    // After a PSM identifier is checked in, local state prefs is updated.
    EXPECT_LT(prev_time, use_case->GetLastKnownPingTimestamp());
  }

  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
}

TEST_F(DeviceActivityClientTest, NetworkDisconnectsWhileWaitingForResponse) {
  // Device active reporting starts check membership on network connect.
  SetWifiNetworkState(shill::kStateOnline);

  // We expect the size of the use cases to be greater than 0.
  EXPECT_GT(device_activity_client_->GetUseCases().size(), 0);

  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kCheckingMembershipOprf);

  // Currently there is at least 1 pending request that has not received it's
  // response.
  EXPECT_GT(test_url_loader_factory_.NumPending(), 0);

  // Disconnect network.
  SetWifiNetworkState(shill::kStateOffline);

  // All pending requests should be cancelled, and our device activity client
  // should get set back to |kIdle|.
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
}

TEST_F(DeviceActivityClientTest, CheckInAfterNextUtcMidnight) {
  // Device active reporting starts check membership on network connect.
  SetWifiNetworkState(shill::kStateOnline);

  // |nonmember_test_case| is used to return psm response bodies for
  // the OPRF, and Query requests. The query request returns nonmember status.
  const psm_rlwe::PrivateMembershipRlweClientRegressionTestData::TestCase&
      nonmember_test_case = psm_test_data_->nonmember_test_case;

  for (auto* use_case : device_activity_client_->GetUseCases()) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    EXPECT_EQ(device_activity_client_->GetState(),
              DeviceActivityClient::State::kCheckingMembershipOprf);

    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFresnelTestEndpoint(kPsmOprfRequestEndpoint),
        GetFresnelOprfResponse(nonmember_test_case), net::HTTP_OK);
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFresnelTestEndpoint(kPsmQueryRequestEndpoint),
        GetFresnelQueryResponse(nonmember_test_case), net::HTTP_OK);
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFresnelTestEndpoint(kPsmImportRequestEndpoint), std::string(),
        net::HTTP_OK);
    task_environment_.RunUntilIdle();
  }

  // Return back to |kIdle| state after a successful check-in.
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);

  task_environment_.FastForwardBy(TimeUntilNextUTCMidnight());
  task_environment_.RunUntilIdle();

  FireTimer();

  // Check that at least 1 network request is pending since the PSM id
  // has NOT been imported for the new UTC period.
  EXPECT_GT(test_url_loader_factory_.NumPending(), 0);

  // Verify state is |kCheckingIn| since local state was updated
  // with the last check in timestamp during the previous day check ins.
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kCheckingIn);

  // Return well formed Import response body for the DAILY use case.
  // The time was forwarded by 1 day, which means only the daily use case will
  // report actives again.
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      GetFresnelTestEndpoint(kPsmImportRequestEndpoint), std::string(),
      net::HTTP_OK);
  task_environment_.RunUntilIdle();

  // Return back to |kIdle| state after successful check-in of daily use case.
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
}

TEST_F(DeviceActivityClientTest, DoNotCheckInTwiceBeforeNextUtcDay) {
  // Device active reporting starts check membership on network connect.
  SetWifiNetworkState(shill::kStateOnline);

  // |nonmember_test_case| is used to return psm response bodies for
  // the OPRF, and Query requests. The query request returns nonmember status.
  const psm_rlwe::PrivateMembershipRlweClientRegressionTestData::TestCase&
      nonmember_test_case = psm_test_data_->nonmember_test_case;

  for (auto* use_case : device_activity_client_->GetUseCases()) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    EXPECT_EQ(device_activity_client_->GetState(),
              DeviceActivityClient::State::kCheckingMembershipOprf);

    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFresnelTestEndpoint(kPsmOprfRequestEndpoint),
        GetFresnelOprfResponse(nonmember_test_case), net::HTTP_OK);
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFresnelTestEndpoint(kPsmQueryRequestEndpoint),
        GetFresnelQueryResponse(nonmember_test_case), net::HTTP_OK);
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFresnelTestEndpoint(kPsmImportRequestEndpoint), std::string(),
        net::HTTP_OK);
    task_environment_.RunUntilIdle();
  }

  // Return back to |kIdle| state after the first successful check-in.
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);

  base::TimeDelta before_utc_meridian =
      TimeUntilNextUTCMidnight() - base::Minutes(1);
  task_environment_.FastForwardBy(before_utc_meridian);
  task_environment_.RunUntilIdle();

  // Trigger attempt to report device active.
  FireTimer();

  // Client should not send any network requests since device is still in same
  // UTC day.
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);

  // Remains in |kIdle| state since the device is still in same UTC day.
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
}

TEST_F(DeviceActivityClientTest, CheckInAfterNextUtcMonth) {
  // Device active reporting starts check membership on network connect.
  SetWifiNetworkState(shill::kStateOnline);

  // |nonmember_test_case| is used to return psm response bodies for
  // the OPRF, and Query requests. The query request returns nonmember status.
  const psm_rlwe::PrivateMembershipRlweClientRegressionTestData::TestCase&
      nonmember_test_case = psm_test_data_->nonmember_test_case;

  for (auto* use_case : device_activity_client_->GetUseCases()) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    EXPECT_EQ(device_activity_client_->GetState(),
              DeviceActivityClient::State::kCheckingMembershipOprf);

    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFresnelTestEndpoint(kPsmOprfRequestEndpoint),
        GetFresnelOprfResponse(nonmember_test_case), net::HTTP_OK);
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFresnelTestEndpoint(kPsmQueryRequestEndpoint),
        GetFresnelQueryResponse(nonmember_test_case), net::HTTP_OK);
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFresnelTestEndpoint(kPsmImportRequestEndpoint), std::string(),
        net::HTTP_OK);
    task_environment_.RunUntilIdle();
  }

  // Return back to |kIdle| state after a successful check-in.
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);

  task_environment_.FastForwardBy(TimeUntilNewUTCMonth());
  task_environment_.RunUntilIdle();

  FireTimer();

  // Check that at least 1 network request is pending since the PSM id
  // has NOT been imported for the new UTC period.
  EXPECT_GT(test_url_loader_factory_.NumPending(), 0);

  // Verify state is |kCheckingIn| since local state was updated
  // with the last check in timestamp during the previous day check ins.
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kCheckingIn);

  // Return well formed Import response body for daily and monthly use case.
  // The time was forwarded to a new month, which means the daily and monthly
  // use cases will report active again.
  for (auto* use_case : device_activity_client_->GetUseCases()) {
    psm_rlwe::RlweUseCase psm_use_case = use_case->GetPsmUseCase();
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(psm_use_case));

    if (psm_use_case == psm_rlwe::RlweUseCase::CROS_FRESNEL_DAILY ||
        psm_use_case == psm_rlwe::RlweUseCase::CROS_FRESNEL_MONTHLY) {
      EXPECT_EQ(device_activity_client_->GetState(),
                DeviceActivityClient::State::kCheckingIn);

      test_url_loader_factory_.SimulateResponseForPendingRequest(
          GetFresnelTestEndpoint(kPsmImportRequestEndpoint), std::string(),
          net::HTTP_OK);
      task_environment_.RunUntilIdle();
    }
  }

  // Return back to |kIdle| state after successful check-in of daily use case.
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
}

// Powerwashing a device resets the local state. This will result in the
// client re-importing a PSM ID, on the same day.
TEST_F(DeviceActivityClientTest, CheckInAgainOnLocalStateReset) {
  // Device active reporting starts check membership on network connect.
  SetWifiNetworkState(shill::kStateOnline);

  // |nonmember_test_case| is used to return psm response bodies for
  // the OPRF, and Query requests. The query request returns nonmember status.
  const psm_rlwe::PrivateMembershipRlweClientRegressionTestData::TestCase&
      nonmember_test_case = psm_test_data_->nonmember_test_case;

  for (auto* use_case : device_activity_client_->GetUseCases()) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    base::Time prev_time = use_case->GetLastKnownPingTimestamp();

    // Mock Successful |kCheckingMembershipOprf|.
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFresnelTestEndpoint(kPsmOprfRequestEndpoint),
        GetFresnelOprfResponse(nonmember_test_case), net::HTTP_OK);

    // Mock Successful |kCheckingMembershipQuery|.
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFresnelTestEndpoint(kPsmQueryRequestEndpoint),
        GetFresnelQueryResponse(nonmember_test_case), net::HTTP_OK);

    // Mock Successful |kCheckingIn|.
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFresnelTestEndpoint(kPsmImportRequestEndpoint), std::string(),
        net::HTTP_OK);
    task_environment_.RunUntilIdle();

    base::Time new_time = use_case->GetLastKnownPingTimestamp();

    // After a PSM identifier is checked in, local state prefs is updated.
    EXPECT_LT(prev_time, new_time);
  }

  // Simulate powerwashing device by removing related local state prefs.
  SimulateLocalStateOnPowerwash();

  // Retrigger |TransitionOutOfIdle| codepath by either firing timer or
  // reconnecting network.
  FireTimer();

  // Verify each use case performs check in successfully after local state prefs
  // is reset.
  for (auto* use_case : device_activity_client_->GetUseCases()) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    // Verify that the |kCheckingIn| state is reached.
    // Indicator is used to verify that we are checking in the PSM ID again
    // after powerwash/recovery scenario.
    EXPECT_EQ(device_activity_client_->GetState(),
              DeviceActivityClient::State::kCheckingMembershipOprf);

    base::Time prev_time = use_case->GetLastKnownPingTimestamp();

    // Mock Successful |kCheckingMembershipOprf|.
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFresnelTestEndpoint(kPsmOprfRequestEndpoint),
        GetFresnelOprfResponse(nonmember_test_case), net::HTTP_OK);

    // Mock Successful |kCheckingMembershipQuery|.
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFresnelTestEndpoint(kPsmQueryRequestEndpoint),
        GetFresnelQueryResponse(nonmember_test_case), net::HTTP_OK);

    // Mock Successful |kCheckingIn|.
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFresnelTestEndpoint(kPsmImportRequestEndpoint), std::string(),
        net::HTTP_OK);
    task_environment_.RunUntilIdle();

    base::Time new_time = use_case->GetLastKnownPingTimestamp();

    // After a PSM identifier is checked in, local state prefs is updated.
    EXPECT_LT(prev_time, new_time);
  }

  // Transitions back to |kIdle| state.
  EXPECT_EQ(device_activity_client_->GetState(),
            DeviceActivityClient::State::kIdle);
}

TEST_F(DeviceActivityClientTest, InitialUmaHistogramStateCount) {
  histogram_tester_.ExpectBucketCount(
      "Ash.DeviceActiveClient.StateCount",
      DeviceActivityClient::State::kCheckingMembershipOprf, 0);
  histogram_tester_.ExpectBucketCount(
      "Ash.DeviceActiveClient.StateCount",
      DeviceActivityClient::State::kCheckingMembershipQuery, 0);
  histogram_tester_.ExpectBucketCount("Ash.DeviceActiveClient.StateCount",
                                      DeviceActivityClient::State::kCheckingIn,
                                      0);
}

TEST_F(DeviceActivityClientTest, UmaHistogramStateCountAfterFirstCheckIn) {
  // Device active reporting starts membership check on network connect.
  SetWifiNetworkState(shill::kStateOnline);

  std::vector<DeviceActiveUseCase*> use_cases =
      device_activity_client_->GetUseCases();

  // |nonmember_test_case| is used to return psm response bodies for
  // the OPRF, and Query requests. The query request returns nonmember status.
  const psm_rlwe::PrivateMembershipRlweClientRegressionTestData::TestCase&
      nonmember_test_case = psm_test_data_->nonmember_test_case;

  for (auto* use_case : use_cases) {
    SCOPED_TRACE(testing::Message()
                 << "PSM use case: "
                 << psm_rlwe::RlweUseCase_Name(use_case->GetPsmUseCase()));

    // Mock Successful |kCheckingMembershipOprf|.
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFresnelTestEndpoint(kPsmOprfRequestEndpoint),
        GetFresnelOprfResponse(nonmember_test_case), net::HTTP_OK);

    // Mock Successful |kCheckingMembershipQuery|.
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFresnelTestEndpoint(kPsmQueryRequestEndpoint),
        GetFresnelQueryResponse(nonmember_test_case), net::HTTP_OK);

    // Mock Successful |kCheckingIn|.
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetFresnelTestEndpoint(kPsmImportRequestEndpoint), std::string(),
        net::HTTP_OK);
    task_environment_.RunUntilIdle();
  }

  histogram_tester_.ExpectBucketCount("Ash.DeviceActiveClient.StateCount",
                                      DeviceActivityClient::State::kCheckingIn,
                                      use_cases.size());
}

}  // namespace device_activity
}  // namespace ash
