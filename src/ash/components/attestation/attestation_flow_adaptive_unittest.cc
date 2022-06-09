// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/attestation/attestation_flow_adaptive.h"

#include <utility>

#include "ash/components/attestation/mock_attestation_flow.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::StrictMock;
using testing::WithArg;

namespace ash {
namespace attestation {

namespace {

constexpr AttestationCertificateProfile kFakeProfile =
    PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE;
constexpr char kFakeUserEmail[] = "fake@gmail.com";
constexpr char kFakeOrigin[] = "fake origin";
constexpr char kFakeKeyName[] = "fake key name";
constexpr char kFakeCert[] = "fake cert";

class TestAttestationFlowFactory : public AttestationFlowFactory {
 public:
  TestAttestationFlowFactory() = default;
  ~TestAttestationFlowFactory() override = default;

  void Initialize(std::unique_ptr<ServerProxy> server_proxy) override {
    ASSERT_TRUE(server_proxy.get());
  }
  AttestationFlow* GetDefault() override { return &mock_default_; }
  AttestationFlow* GetFallback() override { return &mock_fallback_; }
  MockAttestationFlow* GetFallbackMock() { return &mock_fallback_; }
  MockAttestationFlow* GetDefaultMock() { return &mock_default_; }

 private:
  StrictMock<MockAttestationFlow> mock_default_;
  StrictMock<MockAttestationFlow> mock_fallback_;
};

// A mockable fake `AttestationFlowTypeDecider` implementation. This is mocked
// so we can set expectations of the function calls.
class FakeAttestationFlowTypeDecider : public AttestationFlowTypeDecider {
 public:
  FakeAttestationFlowTypeDecider() {
    ON_CALL(*this, CheckType(_, _, _))
        .WillByDefault(
            Invoke(this, &FakeAttestationFlowTypeDecider::FakeCheck));
  }
  ~FakeAttestationFlowTypeDecider() override = default;

  MOCK_METHOD(void,
              CheckType,
              (ServerProxy*,
               AttestationFlowStatusReporter*,
               AttestationFlowTypeCheckCallback));

  // TODO(b/158532239): When adding check of system proxy availability, emulate
  // the real decider by jugding the validity by sub-factors.
  void set_is_default_attestation_valid(bool is_valid) { is_valid_ = is_valid; }
  void set_has_proxy(bool has_proxy) { has_proxy_ = has_proxy; }

 private:
  void FakeCheck(ServerProxy* server_proxy,
                 AttestationFlowStatusReporter* reporter,
                 AttestationFlowTypeCheckCallback callback) {
    EXPECT_TRUE(server_proxy);
    reporter->OnHasProxy(has_proxy_);
    // TODO(b/158532239): Add test after determining the availability of
    // system proxy at runtime.
    reporter->OnIsSystemProxyAvailable(false);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), is_valid_));
  }
  bool is_valid_ = false;
  bool has_proxy_ = false;
};

}  // namespace

class AttestationFlowAdaptiveTest : public testing::Test {
 public:
  AttestationFlowAdaptiveTest() = default;
  ~AttestationFlowAdaptiveTest() override = default;

 protected:
  void ExpectReport(int metric_value, int number) {
    histogram_tester_.ExpectUniqueSample(
        "ChromeOS.Attestation.AttestationFlowStatus", metric_value, number);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
};

TEST_F(AttestationFlowAdaptiveTest, DefaultFlowSuccess) {
  // The ownerships of these testing objects will be transferred to the object
  // under test.
  StrictMock<FakeAttestationFlowTypeDecider>* fake_decider =
      new StrictMock<FakeAttestationFlowTypeDecider>();
  TestAttestationFlowFactory* test_factory = new TestAttestationFlowFactory();

  fake_decider->set_is_default_attestation_valid(true);
  EXPECT_CALL(*fake_decider, CheckType(_, _, _)).Times(1);
  EXPECT_CALL(
      *(test_factory->GetDefaultMock()),
      GetCertificate(kFakeProfile, AccountId::FromUserEmail(kFakeUserEmail),
                     kFakeOrigin, true, kFakeKeyName, _))
      .WillOnce(
          WithArg<5>(Invoke([](AttestationFlow::CertificateCallback callback) {
            std::move(callback).Run(ATTESTATION_SUCCESS, kFakeCert);
          })));
  AttestationStatus result_status;
  std::string result_cert;

  base::RunLoop run_loop;
  auto callback = [](base::RunLoop* run_loop, AttestationStatus* result_status,
                     std::string* result_cert, AttestationStatus status,
                     const std::string& cert) {
    *result_status = status;
    *result_cert = cert;
    run_loop->QuitClosure().Run();
  };
  AttestationFlowAdaptive flow(
      std::make_unique<StrictMock<MockServerProxy>>(),
      std::unique_ptr<AttestationFlowTypeDecider>(fake_decider),
      std::unique_ptr<AttestationFlowFactory>(test_factory));

  flow.GetCertificate(
      kFakeProfile, AccountId::FromUserEmail(kFakeUserEmail), kFakeOrigin, true,
      kFakeKeyName,
      base::BindOnce(callback, &run_loop, &result_status, &result_cert));
  run_loop.Run();
  EXPECT_EQ(result_status, ATTESTATION_SUCCESS);
  EXPECT_EQ(result_cert, kFakeCert);
  // No proxy, run default flow, and default flow works.
  ExpectReport(0b001100, 1);
}

TEST_F(AttestationFlowAdaptiveTest, DefaultFlowFailureAndFallback) {
  // The ownerships of these testing objects will be transferred to the object
  // under test.
  StrictMock<FakeAttestationFlowTypeDecider>* fake_decider =
      new StrictMock<FakeAttestationFlowTypeDecider>();
  TestAttestationFlowFactory* test_factory = new TestAttestationFlowFactory();

  fake_decider->set_is_default_attestation_valid(true);
  EXPECT_CALL(*fake_decider, CheckType(_, _, _)).Times(1);
  EXPECT_CALL(
      *(test_factory->GetDefaultMock()),
      GetCertificate(kFakeProfile, AccountId::FromUserEmail(kFakeUserEmail),
                     kFakeOrigin, true, kFakeKeyName, _))
      .WillOnce(
          WithArg<5>(Invoke([](AttestationFlow::CertificateCallback callback) {
            std::move(callback).Run(ATTESTATION_UNSPECIFIED_FAILURE, "");
          })));
  EXPECT_CALL(
      *(test_factory->GetFallbackMock()),
      GetCertificate(kFakeProfile, AccountId::FromUserEmail(kFakeUserEmail),
                     kFakeOrigin, true, kFakeKeyName, _))
      .WillOnce(
          WithArg<5>(Invoke([](AttestationFlow::CertificateCallback callback) {
            std::move(callback).Run(ATTESTATION_SUCCESS, kFakeCert);
          })));
  AttestationStatus result_status;
  std::string result_cert;

  base::RunLoop run_loop;
  auto callback = [](base::RunLoop* run_loop, AttestationStatus* result_status,
                     std::string* result_cert, AttestationStatus status,
                     const std::string& cert) {
    *result_status = status;
    *result_cert = cert;
    run_loop->QuitClosure().Run();
  };
  AttestationFlowAdaptive flow(
      std::make_unique<StrictMock<MockServerProxy>>(),
      std::unique_ptr<AttestationFlowTypeDecider>(fake_decider),
      std::unique_ptr<AttestationFlowFactory>(test_factory));

  flow.GetCertificate(
      kFakeProfile, AccountId::FromUserEmail(kFakeUserEmail), kFakeOrigin, true,
      kFakeKeyName,
      base::BindOnce(callback, &run_loop, &result_status, &result_cert));
  run_loop.Run();
  EXPECT_EQ(result_status, ATTESTATION_SUCCESS);
  EXPECT_EQ(result_cert, kFakeCert);
  // No proxy, run default flow, and default flow fails, but the fallback works.
  ExpectReport(0b001011, 1);
}

TEST_F(AttestationFlowAdaptiveTest, SkipDefaultFlow) {
  // The ownerships of these testing objects will be transferred to the object
  // under test.
  StrictMock<FakeAttestationFlowTypeDecider>* fake_decider =
      new StrictMock<FakeAttestationFlowTypeDecider>();
  TestAttestationFlowFactory* test_factory = new TestAttestationFlowFactory();

  fake_decider->set_is_default_attestation_valid(false);
  fake_decider->set_has_proxy(true);
  EXPECT_CALL(*fake_decider, CheckType(_, _, _)).Times(1);
  EXPECT_CALL(
      *(test_factory->GetFallbackMock()),
      GetCertificate(kFakeProfile, AccountId::FromUserEmail(kFakeUserEmail),
                     kFakeOrigin, true, kFakeKeyName, _))
      .WillOnce(
          WithArg<5>(Invoke([](AttestationFlow::CertificateCallback callback) {
            std::move(callback).Run(ATTESTATION_SUCCESS, kFakeCert);
          })));
  AttestationStatus result_status;
  std::string result_cert;

  base::RunLoop run_loop;
  auto callback = [](base::RunLoop* run_loop, AttestationStatus* result_status,
                     std::string* result_cert, AttestationStatus status,
                     const std::string& cert) {
    *result_status = status;
    *result_cert = cert;
    run_loop->QuitClosure().Run();
  };
  AttestationFlowAdaptive flow(
      std::make_unique<StrictMock<MockServerProxy>>(),
      std::unique_ptr<AttestationFlowTypeDecider>(fake_decider),
      std::unique_ptr<AttestationFlowFactory>(test_factory));

  flow.GetCertificate(
      kFakeProfile, AccountId::FromUserEmail(kFakeUserEmail), kFakeOrigin, true,
      kFakeKeyName,
      base::BindOnce(callback, &run_loop, &result_status, &result_cert));
  run_loop.Run();
  EXPECT_EQ(result_status, ATTESTATION_SUCCESS);
  EXPECT_EQ(result_cert, kFakeCert);
  // Has proxy, falls back directly, and the fallback works.
  ExpectReport(0b100011, 1);
}

TEST_F(AttestationFlowAdaptiveTest, FallbackTwice) {
  // The ownerships of these testing objects will be transferred to the object
  // under test.
  StrictMock<FakeAttestationFlowTypeDecider>* fake_decider =
      new StrictMock<FakeAttestationFlowTypeDecider>();
  TestAttestationFlowFactory* test_factory = new TestAttestationFlowFactory();

  fake_decider->set_is_default_attestation_valid(false);
  fake_decider->set_has_proxy(true);
  EXPECT_CALL(*fake_decider, CheckType(_, _, _)).Times(2);
  EXPECT_CALL(
      *(test_factory->GetFallbackMock()),
      GetCertificate(kFakeProfile, AccountId::FromUserEmail(kFakeUserEmail),
                     kFakeOrigin, true, kFakeKeyName, _))
      .Times(2)
      .WillRepeatedly(
          WithArg<5>(Invoke([](AttestationFlow::CertificateCallback callback) {
            std::move(callback).Run(ATTESTATION_SUCCESS, kFakeCert);
          })));
  AttestationStatus result_status;
  std::string result_cert;

  base::RunLoop run_loop;
  auto callback = [](base::RunLoop* run_loop, AttestationStatus* result_status,
                     std::string* result_cert, AttestationStatus status,
                     const std::string& cert) {
    *result_status = status;
    *result_cert = cert;
    run_loop->QuitClosure().Run();
  };
  AttestationFlowAdaptive flow(
      std::make_unique<StrictMock<MockServerProxy>>(),
      std::unique_ptr<AttestationFlowTypeDecider>(fake_decider),
      std::unique_ptr<AttestationFlowFactory>(test_factory));

  flow.GetCertificate(
      kFakeProfile, AccountId::FromUserEmail(kFakeUserEmail), kFakeOrigin, true,
      kFakeKeyName,
      base::BindOnce(callback, &run_loop, &result_status, &result_cert));
  run_loop.Run();
  EXPECT_EQ(result_status, ATTESTATION_SUCCESS);
  EXPECT_EQ(result_cert, kFakeCert);

  base::RunLoop run_loop_again;
  flow.GetCertificate(
      kFakeProfile, AccountId::FromUserEmail(kFakeUserEmail), kFakeOrigin, true,
      kFakeKeyName,
      base::BindOnce(callback, &run_loop_again, &result_status, &result_cert));
  run_loop_again.Run();
  EXPECT_EQ(result_status, ATTESTATION_SUCCESS);
  EXPECT_EQ(result_cert, kFakeCert);
  // Has proxy, falls back directly, and the fallback works twice.
  ExpectReport(0b100011, 2);
}

}  // namespace attestation
}  // namespace ash
