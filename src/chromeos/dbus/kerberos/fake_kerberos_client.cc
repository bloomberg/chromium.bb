// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/kerberos/fake_kerberos_client.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "third_party/cros_system_api/dbus/kerberos/dbus-constants.h"

namespace chromeos {
namespace {

// Fake delay for any asynchronous operation.
const auto kTaskDelay = base::TimeDelta::FromMilliseconds(500);

// Posts |callback| on the current thread's task runner, passing it the
// |response| message.
template <class TProto>
void PostProtoResponse(base::OnceCallback<void(const TProto&)> callback,
                       const TProto& response) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::BindOnce(std::move(callback), response), kTaskDelay);
}

// Similar to PostProtoResponse(), but posts |callback| with a proto containing
// only the given |error|.
template <class TProto>
void PostResponse(base::OnceCallback<void(const TProto&)> callback,
                  kerberos::ErrorType error) {
  TProto response;
  response.set_error(error);
  PostProtoResponse(std::move(callback), response);
}

}  // namespace

FakeKerberosClient::FakeKerberosClient() = default;

FakeKerberosClient::~FakeKerberosClient() = default;

void FakeKerberosClient::AddAccount(const kerberos::AddAccountRequest& request,
                                    AddAccountCallback callback) {
  if (!started_) {
    PostResponse(std::move(callback), kerberos::ERROR_DBUS_FAILURE);
    return;
  }

  if (accounts_.find(request.principal_name()) != accounts_.end()) {
    PostResponse(std::move(callback), kerberos::ERROR_DUPLICATE_PRINCIPAL_NAME);
    return;
  }

  accounts_[request.principal_name()] = AccountData();
  PostResponse(std::move(callback), kerberos::ERROR_NONE);
}

void FakeKerberosClient::RemoveAccount(
    const kerberos::RemoveAccountRequest& request,
    RemoveAccountCallback callback) {
  if (!started_) {
    PostResponse(std::move(callback), kerberos::ERROR_DBUS_FAILURE);
    return;
  }

  kerberos::ErrorType error = accounts_.erase(request.principal_name()) == 0
                                  ? kerberos::ERROR_UNKNOWN_PRINCIPAL_NAME
                                  : kerberos::ERROR_NONE;
  PostResponse(std::move(callback), error);
}

void FakeKerberosClient::SetConfig(const kerberos::SetConfigRequest& request,
                                   SetConfigCallback callback) {
  if (!started_) {
    PostResponse(std::move(callback), kerberos::ERROR_DBUS_FAILURE);
    return;
  }

  base::Optional<AccountData> data = GetAccountData(request.principal_name());
  if (!data) {
    PostResponse(std::move(callback), kerberos::ERROR_UNKNOWN_PRINCIPAL_NAME);
    return;
  }

  data->krb5conf = request.krb5conf();
  PostResponse(std::move(callback), kerberos::ERROR_NONE);
}

void FakeKerberosClient::AcquireKerberosTgt(
    const kerberos::AcquireKerberosTgtRequest& request,
    int password_fd,
    AcquireKerberosTgtCallback callback) {
  if (!started_) {
    PostResponse(std::move(callback), kerberos::ERROR_DBUS_FAILURE);
    return;
  }

  base::Optional<AccountData> data = GetAccountData(request.principal_name());
  if (!data) {
    PostResponse(std::move(callback), kerberos::ERROR_UNKNOWN_PRINCIPAL_NAME);
    return;
  }

  // It worked! Magic!
  data->has_tgt = true;
  PostResponse(std::move(callback), kerberos::ERROR_NONE);
}

void FakeKerberosClient::GetKerberosFiles(
    const kerberos::GetKerberosFilesRequest& request,
    GetKerberosFilesCallback callback) {
  if (!started_) {
    PostResponse(std::move(callback), kerberos::ERROR_DBUS_FAILURE);
    return;
  }

  base::Optional<AccountData> data = GetAccountData(request.principal_name());
  if (!data) {
    PostResponse(std::move(callback), kerberos::ERROR_UNKNOWN_PRINCIPAL_NAME);
    return;
  }

  kerberos::GetKerberosFilesResponse response;
  if (data->has_tgt) {
    response.mutable_files()->set_krb5cc("Fake Kerberos credential cache");
    response.mutable_files()->set_krb5conf("Fake Kerberos configuration");
  }
  response.set_error(kerberos::ERROR_NONE);
  PostProtoResponse(std::move(callback), response);
}

void FakeKerberosClient::ConnectToKerberosFileChangedSignal(
    KerberosFilesChangedCallback callback) {
  DCHECK(!kerberos_files_changed_callback_);
  kerberos_files_changed_callback_ = callback;
}

base::Optional<FakeKerberosClient::AccountData>
FakeKerberosClient::GetAccountData(const std::string& principal_name) {
  auto it = accounts_.find(principal_name);
  if (it == accounts_.end())
    return base::nullopt;
  return it->second;
}

KerberosClient::TestInterface* FakeKerberosClient::GetTestInterface() {
  return this;
}

void FakeKerberosClient::FakeKerberosClient::set_started(bool started) {
  started_ = started;
}

bool FakeKerberosClient::FakeKerberosClient::started() const {
  return started_;
}

}  // namespace chromeos
