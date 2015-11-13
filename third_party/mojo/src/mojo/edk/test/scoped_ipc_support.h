// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_MOJO_SRC_MOJO_EDK_TEST_SCOPED_IPC_SUPPORT_H_
#define THIRD_PARTY_MOJO_SRC_MOJO_EDK_TEST_SCOPED_IPC_SUPPORT_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task_runner.h"
#include "mojo/public/cpp/system/macros.h"
#include "third_party/mojo/src/mojo/edk/embedder/master_process_delegate.h"
#include "third_party/mojo/src/mojo/edk/embedder/process_delegate.h"
#include "third_party/mojo/src/mojo/edk/embedder/process_type.h"
#include "third_party/mojo/src/mojo/edk/embedder/scoped_platform_handle.h"
#include "third_party/mojo/src/mojo/edk/embedder/slave_process_delegate.h"

namespace mojo {
namespace test {

namespace internal {

class ScopedIPCSupportHelper {
 public:
  ScopedIPCSupportHelper();
  ~ScopedIPCSupportHelper();

  void Init(embedder::ProcessType process_type,
            embedder::ProcessDelegate* process_delegate,
            scoped_refptr<base::TaskRunner> io_thread_task_runner,
            embedder::ScopedPlatformHandle platform_handle);

  void OnShutdownCompleteImpl();

 private:
  scoped_refptr<base::TaskRunner> io_thread_task_runner_;

  // Set after shut down.
  base::RunLoop run_loop_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ScopedIPCSupportHelper);
};

}  // namespace internal

// A simple class that calls |mojo::embedder::InitIPCSupport()| (with
// |ProcessType::NONE|) on construction and |ShutdownIPCSupport()| on
// destruction (or |ShutdownIPCSupportOnIOThread()| if destroyed on the I/O
// thread).
class ScopedIPCSupport : public embedder::ProcessDelegate {
 public:
  explicit ScopedIPCSupport(
      scoped_refptr<base::TaskRunner> io_thread_task_runner);
  ~ScopedIPCSupport() override;

 private:
  // |ProcessDelegate| implementation:
  // Note: Executed on the I/O thread.
  void OnShutdownComplete() override;

  internal::ScopedIPCSupportHelper helper_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ScopedIPCSupport);
};

// Like |ScopedIPCSupport|, but with |ProcessType::MASTER|. It will (optionally)
// call a callback (on the I/O thread) on receiving |OnSlaveDisconnect()|.
class ScopedMasterIPCSupport : public embedder::MasterProcessDelegate {
 public:
  explicit ScopedMasterIPCSupport(
      scoped_refptr<base::TaskRunner> io_thread_task_runner);
  ScopedMasterIPCSupport(
      scoped_refptr<base::TaskRunner> io_thread_task_runner,
      base::Callback<void(embedder::SlaveInfo slave_info)> on_slave_disconnect);
  ~ScopedMasterIPCSupport() override;

 private:
  // |MasterProcessDelegate| implementation:
  // Note: Executed on the I/O thread.
  void OnShutdownComplete() override;
  void OnSlaveDisconnect(embedder::SlaveInfo slave_info) override;

  internal::ScopedIPCSupportHelper helper_;
  base::Callback<void(embedder::SlaveInfo slave_info)> on_slave_disconnect_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ScopedMasterIPCSupport);
};

// Like |ScopedIPCSupport|, but with |ProcessType::SLAVE|. It will (optionally)
// call a callback (on the I/O thread) on receiving |OnMasterDisconnect()|.
class ScopedSlaveIPCSupport : public embedder::SlaveProcessDelegate {
 public:
  ScopedSlaveIPCSupport(scoped_refptr<base::TaskRunner> io_thread_task_runner,
                        embedder::ScopedPlatformHandle platform_handle);
  ScopedSlaveIPCSupport(scoped_refptr<base::TaskRunner> io_thread_task_runner,
                        embedder::ScopedPlatformHandle platform_handle,
                        base::Closure on_master_disconnect);
  ~ScopedSlaveIPCSupport() override;

 private:
  // |SlaveProcessDelegate| implementation:
  // Note: Executed on the I/O thread.
  void OnShutdownComplete() override;
  void OnMasterDisconnect() override;

  internal::ScopedIPCSupportHelper helper_;
  base::Closure on_master_disconnect_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ScopedSlaveIPCSupport);
};

}  // namespace test
}  // namespace mojo

#endif  // THIRD_PARTY_MOJO_SRC_MOJO_EDK_TEST_SCOPED_IPC_SUPPORT_H_
