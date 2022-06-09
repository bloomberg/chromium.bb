// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/u2f/fake_u2f_client.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "third_party/cros_system_api/dbus/u2f/dbus-constants.h"

namespace chromeos {

// TODO(crbug/1150681): Make this fake more useful.

FakeU2FClient::FakeU2FClient() = default;
FakeU2FClient::~FakeU2FClient() = default;

void FakeU2FClient::IsUvpaa(const u2f::IsUvpaaRequest& request,
                            DBusMethodCallback<u2f::IsUvpaaResponse> callback) {
  u2f::IsUvpaaResponse response;
  response.set_available(false);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(response)));
}

void FakeU2FClient::IsU2FEnabled(
    const u2f::IsUvpaaRequest& request,
    DBusMethodCallback<u2f::IsUvpaaResponse> callback) {
  u2f::IsUvpaaResponse response;
  response.set_available(false);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(response)));
}

void FakeU2FClient::MakeCredential(
    const u2f::MakeCredentialRequest& request,
    DBusMethodCallback<u2f::MakeCredentialResponse> callback) {
  NOTREACHED();
}

void FakeU2FClient::GetAssertion(
    const u2f::GetAssertionRequest& request,
    DBusMethodCallback<u2f::GetAssertionResponse> callback) {
  NOTREACHED();
}

void FakeU2FClient::HasCredentials(
    const u2f::HasCredentialsRequest& request,
    DBusMethodCallback<u2f::HasCredentialsResponse> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), u2f::HasCredentialsResponse()));
}

void FakeU2FClient::HasLegacyU2FCredentials(
    const u2f::HasCredentialsRequest& request,
    DBusMethodCallback<u2f::HasCredentialsResponse> callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), u2f::HasCredentialsResponse()));
}

void FakeU2FClient::CancelWebAuthnFlow(
    const u2f::CancelWebAuthnFlowRequest& request,
    DBusMethodCallback<u2f::CancelWebAuthnFlowResponse> callback) {
  NOTREACHED();
}

}  // namespace chromeos
