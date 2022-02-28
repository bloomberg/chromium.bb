// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_events_observer_base.h"

#include <string>
#include <utility>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromeos/dbus/cros_healthd/cros_healthd_client.h"
#include "chromeos/dbus/cros_healthd/fake_cros_healthd_client.h"
#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::chromeos::cros_healthd::mojom::CrosHealthdAudioObserver;

namespace reporting {
namespace {

class FakeCrosHealthdAudioObserver
    : public CrosHealthdAudioObserver,
      public CrosHealthdEventsObserverBase<CrosHealthdAudioObserver> {
 public:
  FakeCrosHealthdAudioObserver()
      : CrosHealthdEventsObserverBase<CrosHealthdAudioObserver>(this) {}

  FakeCrosHealthdAudioObserver(const FakeCrosHealthdAudioObserver&) = delete;
  FakeCrosHealthdAudioObserver& operator=(const FakeCrosHealthdAudioObserver&) =
      delete;

  ~FakeCrosHealthdAudioObserver() override = default;

  void OnUnderrun() override {
    MetricData metric_data;
    metric_data.mutable_telemetry_data();
    OnEventObserved(std::move(metric_data));
  }

  void OnSevereUnderrun() override {}

  void FlushForTesting() { receiver_.FlushForTesting(); }

 protected:
  void AddObserver() override {
    ::chromeos::cros_healthd::ServiceConnection::GetInstance()
        ->AddAudioObserver(BindNewPipeAndPassRemote());
  }
};

class CrosHealthdEventsObserverBaseTest : public ::testing::Test {
 public:
  CrosHealthdEventsObserverBaseTest() = default;

  CrosHealthdEventsObserverBaseTest(const CrosHealthdEventsObserverBaseTest&) =
      delete;
  CrosHealthdEventsObserverBaseTest& operator=(
      const CrosHealthdEventsObserverBaseTest&) = delete;

  ~CrosHealthdEventsObserverBaseTest() override = default;

  void SetUp() override { ::chromeos::CrosHealthdClient::InitializeFake(); }

  void TearDown() override {
    ::chromeos::CrosHealthdClient::Shutdown();

    // Wait for ServiceConnection to observe the destruction of the client.
    ::chromeos::cros_healthd::ServiceConnection::GetInstance()
        ->FlushForTesting();
  }

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(CrosHealthdEventsObserverBaseTest, Default) {
  FakeCrosHealthdAudioObserver audio_observer;
  MetricData result_metric_data;
  auto cb = base::BindLambdaForTesting([&](MetricData metric_data) {
    result_metric_data = std::move(metric_data);
  });
  audio_observer.SetOnEventObservedCallback(std::move(cb));

  {
    base::RunLoop run_loop;

    audio_observer.SetReportingEnabled(true);
    ::chromeos::cros_healthd::FakeCrosHealthdClient::Get()
        ->EmitAudioUnderrunEventForTesting();
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     run_loop.QuitClosure());
    run_loop.Run();
  }

  // Reporting is enabled.
  ASSERT_TRUE(result_metric_data.has_telemetry_data());

  // Shutdown cros_healthd to simulate crash.
  chromeos::CrosHealthdClient::Shutdown();
  chromeos::cros_healthd::ServiceConnection::GetInstance()->FlushForTesting();
  // Restart cros_healthd.
  chromeos::CrosHealthdClient::InitializeFake();
  audio_observer.FlushForTesting();
  chromeos::cros_healthd::ServiceConnection::GetInstance()->FlushForTesting();

  result_metric_data.Clear();
  {
    base::RunLoop run_loop;

    ::chromeos::cros_healthd::FakeCrosHealthdClient::Get()
        ->EmitAudioUnderrunEventForTesting();
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     run_loop.QuitClosure());
    run_loop.Run();
  }

  // Observer reconnected after crash.
  EXPECT_TRUE(result_metric_data.has_telemetry_data());

  result_metric_data.Clear();
  {
    base::RunLoop run_loop;

    audio_observer.SetReportingEnabled(false);
    ::chromeos::cros_healthd::FakeCrosHealthdClient::Get()
        ->EmitAudioUnderrunEventForTesting();
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     run_loop.QuitClosure());
    run_loop.Run();
  }

  // Reporting is disabled.
  EXPECT_FALSE(result_metric_data.has_telemetry_data());
}
}  // namespace
}  // namespace reporting
