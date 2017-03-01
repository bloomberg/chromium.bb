// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/fingerprint/fingerprint_chromeos.h"

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class FakeFingerprintObserver : public mojom::FingerprintObserver {
 public:
  explicit FakeFingerprintObserver(mojom::FingerprintObserverRequest request)
      : binding_(this, std::move(request)) {}
  ~FakeFingerprintObserver() override {}

  // mojom::FingerprintObserver
  void OnRestarted() override { restarts_++; }

  void OnScanned(uint32_t scan_result, bool is_complete) override { scans_++; }

  void OnAttempt(uint32_t scan_result,
                 const std::vector<std::string>& recognized_user_ids) override {
    attempts_++;
  }

  void OnFailure() override { failures_++; }

  // Test status counts.
  int scans() { return scans_; }
  int attempts() { return attempts_; }
  int restarts() { return restarts_; }
  int failures() { return failures_; }

 private:
  mojo::Binding<mojom::FingerprintObserver> binding_;
  int scans_ = 0;     // Count of scan signal received.
  int attempts_ = 0;  // Count of attempt signal received.
  int restarts_ = 0;  // Count of restart signal received.
  int failures_ = 0;  // Count of failure signal received.

  DISALLOW_COPY_AND_ASSIGN(FakeFingerprintObserver);
};

class FingerprintChromeOSTest : public testing::Test {
 public:
  FingerprintChromeOSTest() {
    fingerprint_ = base::WrapUnique(new FingerprintChromeOS());
  }
  ~FingerprintChromeOSTest() override {}

  FingerprintChromeOS* fingerprint() { return fingerprint_.get(); }

  void GenerateRestartSignal() { fingerprint_->BiodBiometricClientRestarted(); }

  void GenerateScanSignal() {
    fingerprint_->BiometricsScanEventReceived(0, true);
  }

  void GenerateAttemptSignal() {
    std::vector<std::string> user_list;
    fingerprint_->BiometricsAttemptEventReceived(0, user_list);
  }

  void GenerateFailureSignal() { fingerprint_->BiometricsFailureReceived(); }

 private:
  base::MessageLoop message_loop_;
  std::unique_ptr<FingerprintChromeOS> fingerprint_;

  DISALLOW_COPY_AND_ASSIGN(FingerprintChromeOSTest);
};

TEST_F(FingerprintChromeOSTest, FingerprintObserverTest) {
  mojom::FingerprintObserverPtr proxy1;
  FakeFingerprintObserver observer1(mojo::MakeRequest(&proxy1));
  mojom::FingerprintObserverPtr proxy2;
  FakeFingerprintObserver observer2(mojo::MakeRequest(&proxy2));

  fingerprint()->AddFingerprintObserver(std::move(proxy1));
  fingerprint()->AddFingerprintObserver(std::move(proxy2));

  GenerateRestartSignal();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer1.restarts(), 1);
  EXPECT_EQ(observer2.restarts(), 1);

  GenerateScanSignal();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer1.scans(), 1);
  EXPECT_EQ(observer2.scans(), 1);

  GenerateAttemptSignal();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer1.attempts(), 1);
  EXPECT_EQ(observer2.attempts(), 1);

  GenerateFailureSignal();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer1.failures(), 1);
  EXPECT_EQ(observer2.failures(), 1);
}

}  // namespace device
