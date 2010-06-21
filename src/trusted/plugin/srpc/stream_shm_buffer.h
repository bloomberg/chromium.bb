/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// A representation of a shared memory buffer abstraction.  These are used
// to communicate between the renderer and the service runtime.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_STREAM_SHM_BUFFER_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_STREAM_SHM_BUFFER_H_

#include "native_client/src/include/portability.h"
#include "native_client/src/include/nacl_macros.h"

struct NaClGioShmUnbounded;
struct NaClDesc;

namespace plugin {

class StreamShmBuffer {
 public:
  StreamShmBuffer();
  ~StreamShmBuffer();
  int32_t read(int32_t offset, int32_t len, void* buf);
  int32_t write(int32_t offset, int32_t len, void* buf);
  NaClDesc* shm(int32_t* size) const;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(StreamShmBuffer);
  NaClGioShmUnbounded* shmbufp_;
};

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_STREAM_SHM_BUFFER_H_
