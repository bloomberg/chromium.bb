// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_NACL_IO_SOCKET_PACKET_H_
#define LIBRARIES_NACL_IO_SOCKET_PACKET_H_

#include "nacl_io/fifo_interface.h"
#include "ppapi/c/pp_resource.h"

#include "sdk_util/macros.h"

namespace nacl_io {

class PepperInterface;

// NOTE: The Packet class always owns the buffer and address.
class Packet {
 public:
  explicit Packet(PepperInterface* ppapi);
  ~Packet();

  // Copy the buffer, and address reference
  void Copy(const void* buffer, size_t len, PP_Resource addr);

  char* buffer() { return buffer_; }
  PP_Resource addr() { return addr_; }
  size_t len() { return len_; }

 private:
  PepperInterface* ppapi_;
  PP_Resource addr_;
  char* buffer_;
  size_t len_;

  DISALLOW_COPY_AND_ASSIGN(Packet);
};

}  // namespace nacl_io

#endif  // LIBRARIES_NACL_IO_SOCKET_PACKET_H_
