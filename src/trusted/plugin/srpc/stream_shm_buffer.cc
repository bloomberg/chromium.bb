/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include "native_client/src/trusted/plugin/srpc/stream_shm_buffer.h"

#include <stdio.h>
#include <string.h>
#include <limits>

#include "native_client/src/include/checked_cast.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"

#include "native_client/src/trusted/plugin/srpc/utility.h"

#include "native_client/src/trusted/service_runtime/gio_shm_unbounded.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/sel_util.h"

using nacl::assert_cast;

namespace plugin {

StreamShmBuffer::StreamShmBuffer() {
  shmbufp_ = reinterpret_cast<NaClGioShmUnbounded *>(malloc(sizeof *shmbufp_));
  // If NULL == shmbufp_, object is not completely initialized.
  // Therefore all member functions need to check before running.
  if (NULL == shmbufp_) {
    PLUGIN_PRINTF(("StreamShmBuffer: malloc failed\n"));
    return;
  }
  if (!NaClGioShmUnboundedCtor(shmbufp_)) {
    PLUGIN_PRINTF(("StreamShmBuffer: NaClGioShmUnboundedCtor failed\n"));
    free(shmbufp_);
    shmbufp_ = NULL;
  }
}

StreamShmBuffer::~StreamShmBuffer() {
  if (NULL == shmbufp_) {
    return;
  }
  shmbufp_->base.vtbl->Dtor(&shmbufp_->base);
  free(shmbufp_);
}

int32_t StreamShmBuffer::write(int32_t offset, int32_t len, void* buf) {
  // Errors are reported to NPAPI by using a negative return value.
  // If the constructor failed to set things up corretly, we return failure.
  if (NULL == shmbufp_) {
    return -1;
  }
  // If the offset or length is negative, return failure.
  if (0 > offset || 0 > len) {
    return -1;
  }
  if (0 > shmbufp_->base.vtbl->Seek(&shmbufp_->base, offset, SEEK_SET)) {
    return -1;
  }
  ssize_t rv = shmbufp_->base.vtbl->Write(&shmbufp_->base, buf, len);
  if (rv != len) {
    PLUGIN_PRINTF(("StreamShmBuffer::write returned %" NACL_PRIdS
                   ", not %" NACL_PRIdS "\n",
                   rv, static_cast<ssize_t>(len)));
  }
  return static_cast<int32_t>(rv);
}

int32_t StreamShmBuffer::read(int32_t offset, int32_t len, void* buf) {
  // Errors are reported to NPAPI by using a negative return value.
  // If the constructor failed to set things up corretly, we return failure.
  if (NULL == shmbufp_) {
    return -1;
  }
  // If the offset or length is negative, return failure.
  if (0 > offset || 0 > len) {
    return -1;
  }
  if (0 > shmbufp_->base.vtbl->Seek(&shmbufp_->base, offset, SEEK_SET)) {
    return -1;
  }
  ssize_t rv = shmbufp_->base.vtbl->Read(&shmbufp_->base, buf, len);
  if (rv != len) {
    PLUGIN_PRINTF(("StreamShmBuffer::read returned %" NACL_PRIdS
                   ", not %" NACL_PRIdS "\n",
                   rv, static_cast<ssize_t>(len)));
  }
  return static_cast<int32_t>(rv);
}

NaClDesc* StreamShmBuffer::shm(int32_t* size) const {
  size_t actual_size;
  NaClDesc *ndp;

  ndp = NaClGioShmUnboundedGetNaClDesc(shmbufp_, &actual_size);
  if (static_cast<size_t>(std::numeric_limits<int32_t>::max()) < actual_size) {
    // fail -- not representable
    *size = 0;
    return NULL;
  }
  *size = assert_cast<int32_t>(actual_size);
  return ndp;
}

}  // namespace plugin
