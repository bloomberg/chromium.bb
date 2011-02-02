// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_FLIP_SERVER_STREAMER_INTERFACE_
#define NET_TOOLS_FLIP_SERVER_STREAMER_INTERFACE_

#include <string>

#include "net/tools/flip_server/sm_interface.h"

namespace net {

class FlipAcceptor;
class MemCacheIter;
class SMConnection;
class EpollServer;

class StreamerSM : public SMInterface {
 public:
  StreamerSM(SMConnection* connection,
             SMInterface* sm_other_interface,
             EpollServer* epoll_server,
             FlipAcceptor* acceptor);
  virtual ~StreamerSM();

  void InitSMInterface(SMInterface* sm_other_interface,
                       int32 server_idx);
  void InitSMConnection(SMConnectionPoolInterface* connection_pool,
                        SMInterface* sm_interface,
                        EpollServer* epoll_server,
                        int fd,
                        std::string server_ip,
                        std::string server_port,
                        std::string remote_ip,
                        bool use_ssl);

  size_t ProcessReadInput(const char* data, size_t len);
  size_t ProcessWriteInput(const char* data, size_t len);
  bool MessageFullyRead() const { return false; }
  void SetStreamID(uint32 stream_id) {}
  bool Error() const { return false; }
  const char* ErrorAsString() const { return "(none)"; }
  void Reset();
  void ResetForNewInterface(int32 server_idx) {}
  void ResetForNewConnection() { sm_other_interface_->Reset(); }
  void Cleanup() {}
  int PostAcceptHook();
  void NewStream(uint32 stream_id, uint32 priority,
                 const std::string& filename) {}
  void AddToOutputOrder(const MemCacheIter& mci) {}
  void SendEOF(uint32 stream_id) {}
  void SendErrorNotFound(uint32 stream_id) {}
  void SendOKResponse(uint32 stream_id, std::string output) {}
  size_t SendSynStream(uint32 stream_id, const BalsaHeaders& headers) {
    return 0;
  }
  size_t SendSynReply(uint32 stream_id, const BalsaHeaders& headers) {
    return 0;
  }
  void SendDataFrame(uint32 stream_id, const char* data, int64 len,
                     uint32 flags, bool compress) {}

 private:
  void SendEOFImpl(uint32 stream_id) {}
  void SendErrorNotFoundImpl(uint32 stream_id) {}
  void SendOKResponseImpl(uint32 stream_id, std::string* output) {}
  size_t SendSynReplyImpl(uint32 stream_id, const BalsaHeaders& headers) {
    return 0;
  }
  size_t SendSynStreamImpl(uint32 stream_id, const BalsaHeaders& headers) {
    return 0;
  }
  void SendDataFrameImpl(uint32 stream_id, const char* data, int64 len,
                         uint32 flags, bool compress) {}
  void GetOutput() {}

  SMConnection* connection_;
  SMInterface* sm_other_interface_;
  EpollServer* epoll_server_;
  FlipAcceptor* acceptor_;
};

}  // namespace net

#endif  // NET_TOOLS_FLIP_SERVER_STREAMER_INTERFACE_

