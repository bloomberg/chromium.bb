// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/macros.h"
#include "ppapi/proxy/ppapi_proxy_export.h"

namespace ppapi {
namespace proxy {

class PPAPI_PROXY_EXPORT UDPSocketResourceConstants {
 public:
  // The maximum number of bytes that each
  // PpapiPluginMsg_PPBUDPSocket_PushRecvResult message is allowed to carry.
  static const int32_t kMaxReadSize;
  // The maximum number of bytes that each PpapiHostMsg_PPBUDPSocket_SendTo
  // message is allowed to carry.
  static const int32_t kMaxWriteSize;

  // The maximum number that we allow for setting
  // PP_UDPSOCKET_OPTION_SEND_BUFFER_SIZE. This number is only for input
  // argument sanity check, it doesn't mean the browser guarantees to support
  // such a buffer size.
  static const int32_t kMaxSendBufferSize;
  // The maximum number that we allow for setting
  // PP_UDPSOCKET_OPTION_RECV_BUFFER_SIZE. This number is only for input
  // argument sanity check, it doesn't mean the browser guarantees to support
  // such a buffer size.
  static const int32_t kMaxReceiveBufferSize;

  // The maximum number of received packets that we allow instances of this
  // class to buffer.
  static const size_t kPluginReceiveBufferSlots;
  // The maximum number of buffers that we allow instances of this class to be
  // sending before we block the plugin.
  static const size_t kPluginSendBufferSlots;

 private:
  DISALLOW_COPY_AND_ASSIGN(UDPSocketResourceConstants);
};

}  // namespace proxy
}  // namespace ppapi
