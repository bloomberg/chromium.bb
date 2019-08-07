// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_HOST_BACKEND_DELEGATE_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_HOST_BACKEND_DELEGATE_H_

#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/services/multidevice_setup/host_backend_delegate.h"

namespace chromeos {

namespace multidevice_setup {

// Test HostBackendDelegate implementation.
class FakeHostBackendDelegate : public HostBackendDelegate {
 public:
  FakeHostBackendDelegate();
  ~FakeHostBackendDelegate() override;

  // Changes the backend host to |host_device_on_backend| and notifies
  // observers.
  void NotifyHostChangedOnBackend(
      const base::Optional<multidevice::RemoteDeviceRef>&
          host_device_on_backend);

  void NotifyBackendRequestFailed();

  size_t num_attempt_to_set_calls() { return num_attempt_to_set_calls_; }

  // HostBackendDelegate:
  void AttemptToSetMultiDeviceHostOnBackend(
      const base::Optional<multidevice::RemoteDeviceRef>& host_device) override;
  bool HasPendingHostRequest() override;
  base::Optional<multidevice::RemoteDeviceRef> GetPendingHostRequest()
      const override;
  base::Optional<multidevice::RemoteDeviceRef> GetMultiDeviceHostFromBackend()
      const override;

 private:
  size_t num_attempt_to_set_calls_ = 0u;
  base::Optional<base::Optional<multidevice::RemoteDeviceRef>>
      pending_host_request_;
  base::Optional<multidevice::RemoteDeviceRef> host_device_on_backend_;

  DISALLOW_COPY_AND_ASSIGN(FakeHostBackendDelegate);
};

// Test HostBackendDelegate::Observer implementation.
class FakeHostBackendDelegateObserver : public HostBackendDelegate::Observer {
 public:
  FakeHostBackendDelegateObserver();
  ~FakeHostBackendDelegateObserver() override;

  size_t num_changes_on_backend() const { return num_changes_on_backend_; }
  size_t num_failed_backend_requests() const {
    return num_failed_backend_requests_;
  }
  size_t num_pending_host_request_changes() const {
    return num_pending_host_request_changes_;
  }

 private:
  // HostBackendDelegate::Observer:
  void OnHostChangedOnBackend() override;
  void OnBackendRequestFailed() override;
  void OnPendingHostRequestChange() override;

  size_t num_changes_on_backend_ = 0u;
  size_t num_failed_backend_requests_ = 0u;
  size_t num_pending_host_request_changes_ = 0u;

  DISALLOW_COPY_AND_ASSIGN(FakeHostBackendDelegateObserver);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_HOST_BACKEND_DELEGATE_H_
