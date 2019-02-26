// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/upstart_client.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"

namespace chromeos {

namespace {

constexpr char kUpstartServiceName[] = "com.ubuntu.Upstart";
constexpr char kUpstartJobInterface[] = "com.ubuntu.Upstart0_6.Job";
constexpr char kStartMethod[] = "Start";
constexpr char kRestartMethod[] = "Restart";
constexpr char kStopMethod[] = "Stop";

constexpr char kUpstartJobsPath[] = "/com/ubuntu/Upstart/jobs/";
constexpr char kAuthPolicyJob[] = "authpolicyd";
constexpr char kMediaAnalyticsJob[] = "rtanalytics";

class UpstartClientImpl : public UpstartClient {
 public:
  UpstartClientImpl() : weak_ptr_factory_(this) {}

  ~UpstartClientImpl() override = default;

  // UpstartClient overrides:
  void StartJob(const std::string& job,
                const std::vector<std::string>& upstart_env,
                VoidDBusMethodCallback callback) override {
    CallJobMethod(job, kStartMethod, upstart_env, std::move(callback));
  }

  void StopJob(const std::string& job,
               VoidDBusMethodCallback callback) override {
    CallJobMethod(job, kStopMethod, {}, std::move(callback));
  }

  void StartAuthPolicyService() override {
    StartJob(kAuthPolicyJob, {}, EmptyVoidDBusMethodCallback());
  }

  void RestartAuthPolicyService() override {
    CallJobMethod(kAuthPolicyJob, kRestartMethod, {},
                  EmptyVoidDBusMethodCallback());
  }

  void StartMediaAnalytics(const std::vector<std::string>& upstart_env,
                           VoidDBusMethodCallback callback) override {
    StartJob(kMediaAnalyticsJob, upstart_env, std::move(callback));
  }

  void RestartMediaAnalytics(VoidDBusMethodCallback callback) override {
    CallJobMethod(kMediaAnalyticsJob, kRestartMethod, {}, std::move(callback));
  }

  void StopMediaAnalytics() override {
    StopJob(kMediaAnalyticsJob, EmptyVoidDBusMethodCallback());
  }

  void StopMediaAnalytics(VoidDBusMethodCallback callback) override {
    StopJob(kMediaAnalyticsJob, std::move(callback));
  }
 protected:
  void Init(dbus::Bus* bus) override {
    bus_ = bus;
  }

 private:
  void CallJobMethod(const std::string& job,
                     const std::string& method,
                     const std::vector<std::string>& upstart_env,
                     VoidDBusMethodCallback callback) {
    dbus::ObjectProxy* job_proxy = bus_->GetObjectProxy(
        kUpstartServiceName, dbus::ObjectPath(kUpstartJobsPath + job));
    dbus::MethodCall method_call(kUpstartJobInterface, method);
    dbus::MessageWriter writer(&method_call);
    writer.AppendArrayOfStrings(upstart_env);
    writer.AppendBool(true /* wait for response */);
    job_proxy->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&UpstartClientImpl::OnVoidMethod,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnVoidMethod(VoidDBusMethodCallback callback, dbus::Response* response) {
    std::move(callback).Run(response);
  }

  dbus::Bus* bus_ = nullptr;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<UpstartClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(UpstartClientImpl);
};

}  // namespace

UpstartClient::UpstartClient() = default;

UpstartClient::~UpstartClient() = default;

// static
UpstartClient* UpstartClient::Create() {
  return new UpstartClientImpl();
}

}  // namespace chromeos
