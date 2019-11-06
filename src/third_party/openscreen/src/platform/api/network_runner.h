// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_NETWORK_RUNNER_H_
#define PLATFORM_API_NETWORK_RUNNER_H_

#include <array>
#include <cstdint>
#include <functional>
#include <memory>

#include "platform/api/task_runner.h"
#include "platform/api/time.h"
#include "platform/api/udp_read_callback.h"
#include "platform/api/udp_socket.h"
#include "platform/base/error.h"
#include "platform/base/ip_address.h"

namespace openscreen {
namespace platform {

// A thread-safe API that allows for posting tasks and coordinating socket IO.
// The underlying implementation may be single or multi-threaded, and all
// complication should be handled by either the implementation class or the
// NetworkRunnerFactory method. The underlying implementation must give the
// following guarantees:
// (1) Tasks shall not overlap in time/CPU.
// (2) Tasks shall run sequentially, e.g. posting task A then B implies
//     that A shall run before B.
// (3) Network callbacks shall not overlap neither any other network callbacks
//     nor tasks submitted directly via PostTask or PostTaskWithDelay.
// NOTE: We do not make any assumptions about what thread tasks shall run on.
// TODO(rwkeane): Investigate having NetworkRunner not extend TaskRunner to
// avoid a vtable lookup.
class NetworkRunner : public TaskRunner {
 public:
  using TaskRunner::Task;
  ~NetworkRunner() override = default;

  // Waits for |socket| to be readable and then posts and then runs |callback|
  // with the received data.  The actual network operations may occur on a
  // separate thread but |callback| will be not overlap any other tasks posted
  // to this NetworkRunner (i.e. |callback| will be run as if PostTask were
  // called on |callback->OnRead|, modulo syntax).  This will continue to wait
  // for more packets until CancelReadAll is called on the same |socket|.
  virtual Error ReadRepeatedly(UdpSocket* socket,
                               UdpReadCallback* callback) = 0;

  // Cancels any pending wait on reading |socket|. Returns false only if the
  // socket was not yet being watched, and true if the operation is successful
  // and the socket is no longer watched.
  // TODO(rwkeane): Make this return void and either include a DCHECK inside of
  // the implementation or allow failure with no return code.
  virtual bool CancelRead(UdpSocket* socket) = 0;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_NETWORK_RUNNER_H_
