// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_PLATFORM_CLIENT_H_
#define PLATFORM_API_PLATFORM_CLIENT_H_

#include "platform/base/macros.h"

namespace openscreen {
namespace platform {

class TaskRunner;

// A PlatformClient is an access point for all singletons used by the platform
// to which it pertains. The static SetInstance method is to be called before
// library use begins, and the ShutDown() method should be called to deallocate
// the platform library's global singletons (for example to save memory when
// libcast is not in use).
class PlatformClient {
 public:
  // Returns the TaskRunner associated with this PlatformClient.
  // NOTE: This method is expected to be thread safe.
  virtual TaskRunner* task_runner() = 0;

  // Shuts down and deletes the PlatformClient instance currently stored as a
  // singleton. This method is expected to be called before program exit. After
  // calling this method, if the client wishes to continue using the platform
  // library, a new singleton must be created.
  // NOTE: This method is NOT thread safe and should only be called from the
  // embedder thread.
  static void ShutDown();

  static PlatformClient* GetInstance() { return client_; }

  OSP_DISALLOW_COPY_AND_ASSIGN(PlatformClient);

 protected:
  PlatformClient();
  virtual ~PlatformClient();

  // This method is expected to be called from child implementations in order to
  // set the singleton instance. It should only be called from the embedder
  // thread. Client should be a new instance create via 'new' and ownership of
  // this instance will be transferred to this class.
  // NOTE: This method is NOT thread safe and should only be called from the
  // embedder thread.
  static void SetInstance(PlatformClient* client);

 private:
  static PlatformClient* client_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_PLATFORM_CLIENT_H_
