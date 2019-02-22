// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_CONNECT_TO_DEVICE_OPERATION_BASE_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_CONNECT_TO_DEVICE_OPERATION_BASE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/services/secure_channel/connect_to_device_operation.h"
#include "chromeos/services/secure_channel/device_id_pair.h"
#include "chromeos/services/secure_channel/public/cpp/shared/connection_priority.h"

namespace chromeos {

namespace secure_channel {

// Base class for concrete ConnectToDeviceOperation implementations.
// ConnectToDeviceOperationBase objects take a device to which to connect as a
// parameter to their constructors and immediately attempt a connection to the
// device when instantiated.
//
// Derived classes should alert clients of success/failure by invoking the
// OnSuccessfulConnectionAttempt() or OnFailedConnectionAttempt() functions.
template <typename FailureDetailType>
class ConnectToDeviceOperationBase
    : public ConnectToDeviceOperation<FailureDetailType> {
 protected:
  ConnectToDeviceOperationBase(
      typename ConnectToDeviceOperation<
          FailureDetailType>::ConnectionSuccessCallback success_callback,
      typename ConnectToDeviceOperation<
          FailureDetailType>::ConnectionFailedCallback failure_callback,
      const DeviceIdPair& device_id_pair,
      ConnectionPriority connection_priority,
      scoped_refptr<base::TaskRunner> task_runner =
          base::ThreadTaskRunnerHandle::Get())
      : ConnectToDeviceOperation<FailureDetailType>(std::move(success_callback),
                                                    std::move(failure_callback),
                                                    connection_priority),
        device_id_pair_(device_id_pair),
        task_runner_(task_runner),
        weak_ptr_factory_(this) {
    // Attempt a connection; however, post this as a task to be run after the
    // constructor is finished. This ensures that the derived type is fully
    // constructed before a virtual function is invoked.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ConnectToDeviceOperationBase<
                           FailureDetailType>::AttemptConnectionToDevice,
                       weak_ptr_factory_.GetWeakPtr(), connection_priority));
  }

  ~ConnectToDeviceOperationBase() override = default;

  void AttemptConnectionToDevice(ConnectionPriority connection_priority) {
    has_started_connection_attempt_ = true;
    PerformAttemptConnectionToDevice(connection_priority);
  }

  void CancelInternal() override {
    if (has_started_connection_attempt_)
      PerformCancellation();
  }

  void UpdateConnectionPriorityInternal(
      ConnectionPriority connection_priority) override {
    if (has_started_connection_attempt_)
      PerformUpdateConnectionPriority(connection_priority);
  }

  virtual void PerformAttemptConnectionToDevice(
      ConnectionPriority connection_priority) = 0;
  virtual void PerformCancellation() = 0;
  virtual void PerformUpdateConnectionPriority(
      ConnectionPriority connection_priority) = 0;

  const DeviceIdPair& device_id_pair() const { return device_id_pair_; }

 private:
  const DeviceIdPair& device_id_pair_;
  scoped_refptr<base::TaskRunner> task_runner_;
  bool has_started_connection_attempt_ = false;
  base::WeakPtrFactory<ConnectToDeviceOperationBase> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ConnectToDeviceOperationBase);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_CONNECT_TO_DEVICE_OPERATION_BASE_H_
