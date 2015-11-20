// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_BATTOR_AGENT_BATTOR_CONNECTION_H_
#define TOOLS_BATTOR_AGENT_BATTOR_CONNECTION_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "device/serial/serial_io_handler.h"

namespace battor {

// A BattOrConnection is a serial connection to the BattOr. It's essentially
// just a thin wrapper around device::serial::SerialIoHandler that provides
// reasonable defaults for the serial connection for the BattOr and provides a
// synchronous interface.
//
// The serial connection remains open for the lifetime of the object.
class BattOrConnection {
 public:
  // Creates a BattOrConnection to the BattOr at the given path.
  // If creation is unsuccessful, a null pointer is returned.
  static scoped_ptr<BattOrConnection> Create(const std::string& path);
  virtual ~BattOrConnection();

 private:
  BattOrConnection();

  // IO handler capable of reading from and writing to the serial connection.
  scoped_refptr<device::SerialIoHandler> io_handler_;

  DISALLOW_COPY_AND_ASSIGN(BattOrConnection);
};

}  // namespace battor

#endif  // TOOLS_BATTOR_AGENT_BATTOR_CONNECTION_H_
