// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_UDP_UDP_DATA_TRANSFER_PARAM_H_
#define NET_UDP_UDP_DATA_TRANSFER_PARAM_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/net_log.h"

namespace net {

class IPEndPoint;

// NetLog parameter to describe a UDP receive/send event.  Each event has a
// byte count, and may optionally have transferred bytes and an IPEndPoint as
// well.
class UDPDataTransferNetLogParam : public NetLog::EventParameters {
 public:
  // |bytes| are only logged when |log_bytes| is non-NULL.
  // |address| may be NULL.
  UDPDataTransferNetLogParam(int byte_count,
                             const char* bytes,
                             bool log_bytes,
                             const IPEndPoint* address);

  virtual base::Value* ToValue() const OVERRIDE;

 protected:
  virtual ~UDPDataTransferNetLogParam();

 private:
  const int byte_count_;
  const std::string hex_encoded_bytes_;
  scoped_ptr<IPEndPoint> address_;
};

}  // namespace net

#endif  // NET_UDP_UDP_DATA_TRANSFER_PARAM_H_
