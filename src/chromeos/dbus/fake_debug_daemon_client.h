// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_DEBUG_DAEMON_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_DEBUG_DAEMON_CLIENT_H_

#include <stdint.h>
#include <sys/types.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/debug_daemon_client.h"

namespace chromeos {

// The DebugDaemonClient implementation used on Linux desktop,
// which does nothing.
class COMPONENT_EXPORT(CHROMEOS_DBUS) FakeDebugDaemonClient
    : public DebugDaemonClient {
 public:
  FakeDebugDaemonClient();
  ~FakeDebugDaemonClient() override;

  void Init(dbus::Bus* bus) override;
  void DumpDebugLogs(bool is_compressed,
                     int file_descriptor,
                     VoidDBusMethodCallback callback) override;
  void SetDebugMode(const std::string& subsystem,
                    VoidDBusMethodCallback callback) override;
  std::string GetTracingAgentName() override;
  std::string GetTraceEventLabel() override;
  void StartAgentTracing(const base::trace_event::TraceConfig& trace_config,
                         StartAgentTracingCallback callback) override;
  void StopAgentTracing(StopAgentTracingCallback callback) override;
  void SetStopAgentTracingTaskRunner(
      scoped_refptr<base::TaskRunner> task_runner) override;
  void GetRoutes(
      bool numeric,
      bool ipv6,
      DBusMethodCallback<std::vector<std::string>> callback) override;
  void GetNetworkStatus(DBusMethodCallback<std::string> callback) override;
  void GetModemStatus(DBusMethodCallback<std::string> callback) override;
  void GetWiMaxStatus(DBusMethodCallback<std::string> callback) override;
  void GetNetworkInterfaces(DBusMethodCallback<std::string> callback) override;
  void GetPerfOutput(base::TimeDelta duration,
                     const std::vector<std::string>& perf_args,
                     int file_descriptor,
                     DBusMethodCallback<uint64_t> callback) override;
  void GetScrubbedLogs(const GetLogsCallback& callback) override;
  void GetScrubbedBigLogs(const GetLogsCallback& callback) override;
  void GetAllLogs(const GetLogsCallback& callback) override;
  void GetLog(const std::string& log_name,
              DBusMethodCallback<std::string> callback) override;
  void GetUserLogFiles(const GetLogsCallback& callback) override;
  void TestICMP(const std::string& ip_address,
                const TestICMPCallback& callback) override;
  void TestICMPWithOptions(const std::string& ip_address,
                           const std::map<std::string, std::string>& options,
                           const TestICMPCallback& callback) override;
  void UploadCrashes() override;
  void EnableDebuggingFeatures(
      const std::string& password,
      const EnableDebuggingCallback& callback) override;
  void QueryDebuggingFeatures(
      const QueryDevFeaturesCallback& callback) override;
  void RemoveRootfsVerification(
      const EnableDebuggingCallback& callback) override;
  void WaitForServiceToBeAvailable(
      WaitForServiceToBeAvailableCallback callback) override;
  void SetOomScoreAdj(const std::map<pid_t, int32_t>& pid_to_oom_score_adj,
                      const SetOomScoreAdjCallback& callback) override;
  void CupsAddManuallyConfiguredPrinter(
      const std::string& name,
      const std::string& uri,
      const std::string& ppd_contents,
      CupsAddPrinterCallback callback) override;
  void CupsAddAutoConfiguredPrinter(const std::string& name,
                                    const std::string& uri,
                                    CupsAddPrinterCallback callback) override;
  void CupsRemovePrinter(const std::string& name,
                         const CupsRemovePrinterCallback& callback,
                         const base::Closure& error_callback) override;
  void StartConcierge(ConciergeCallback callback) override;
  void StopConcierge(ConciergeCallback callback) override;
  void SetRlzPingSent(SetRlzPingSentCallback callback) override;
  void SetSchedulerConfiguration(const std::string& config_name,
                                 VoidDBusMethodCallback callback) override;

  // Sets debugging features mask for testing.
  virtual void SetDebuggingFeaturesStatus(int features_mask);

  // Changes the behavior of WaitForServiceToBeAvailable(). This method runs
  // pending callbacks if is_available is true.
  void SetServiceIsAvailable(bool is_available);

  const std::string& scheduler_configuration_name() const {
    return scheduler_configuration_name_;
  }

 private:
  int features_mask_;

  bool service_is_available_;
  std::vector<WaitForServiceToBeAvailableCallback>
      pending_wait_for_service_to_be_available_callbacks_;
  std::set<std::string> printers_;
  std::string scheduler_configuration_name_;

  DISALLOW_COPY_AND_ASSIGN(FakeDebugDaemonClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_DEBUG_DAEMON_CLIENT_H_
