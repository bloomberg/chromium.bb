// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/fingerprint/fingerprint_impl_chromeos.h"

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
  int Scans() { return scans_; }
  int Attempts() { return attempts_; }
  int Restarts() { return restarts_; }
  int Failures() { return failures_; }

 private:
  mojo::Binding<mojom::FingerprintObserver> binding_;
  int scans_ = 0;     // Count of scan signal received.
  int attempts_ = 0;  // Count of attempt signal received.
  int restarts_ = 0;  // Count of restart signal received.
  int failures_ = 0;  // Count of failure signal received.

  DISALLOW_COPY_AND_ASSIGN(FakeFingerprintObserver);
};

class FingerprintImplChromeOSTest : public testing::Test {
 public:
  FingerprintImplChromeOSTest() {
    fingerprint_ = base::WrapUnique(new FingerprintImplChromeOS());
  }
  ~FingerprintImplChromeOSTest() override {}

  FingerprintImplChromeOS* fingerprint() { return fingerprint_.get(); }

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
  std::unique_ptr<FingerprintImplChromeOS> fingerprint_;

  DISALLOW_COPY_AND_ASSIGN(FingerprintImplChromeOSTest);
};

TEST_F(FingerprintImplChromeOSTest, FingerprintObserverTest) {
  mojom::FingerprintObserverPtr proxy1;
  FakeFingerprintObserver observer1(mojo::MakeRequest(&proxy1));
  mojom::FingerprintObserverPtr proxy2;
  FakeFingerprintObserver observer2(mojo::MakeRequest(&proxy2));

  fingerprint()->AddFingerprintObserver(std::move(proxy1));
  fingerprint()->AddFingerprintObserver(std::move(proxy2));

  GenerateRestartSignal();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer1.Restarts(), 1);
  EXPECT_EQ(observer2.Restarts(), 1);

  GenerateScanSignal();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer1.Scans(), 1);
  EXPECT_EQ(observer2.Scans(), 1);

  GenerateAttemptSignal();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer1.Attempts(), 1);
  EXPECT_EQ(observer2.Attempts(), 1);

  GenerateFailureSignal();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(observer1.Failures(), 1);
  EXPECT_EQ(observer2.Failures(), 1);
}

}  // namespace device
