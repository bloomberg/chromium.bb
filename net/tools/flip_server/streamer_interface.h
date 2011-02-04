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

  void AddToOutputOrder(const MemCacheIter& mci) {}

  virtual void InitSMInterface(SMInterface* sm_other_interface,
                               int32 server_idx);
  virtual void InitSMConnection(SMConnectionPoolInterface* connection_pool,
                                SMInterface* sm_interface,
                                EpollServer* epoll_server,
                                int fd,
                                std::string server_ip,
                                std::string server_port,
                                std::string remote_ip,
                                bool use_ssl);

  virtual size_t ProcessReadInput(const char* data, size_t len);
  virtual size_t ProcessWriteInput(const char* data, size_t len);
  virtual bool MessageFullyRead() const;
  virtual void SetStreamID(uint32 stream_id) {}
  virtual bool Error() const;
  virtual const char* ErrorAsString() const;
  virtual void Reset();
  virtual void ResetForNewInterface(int32 server_idx) {}
  virtual void ResetForNewConnection();
  virtual void Cleanup() {}
  virtual int PostAcceptHook();
  virtual void NewStream(uint32 stream_id, uint32 priority,
                         const std::string& filename) {}
  virtual void SendEOF(uint32 stream_id) {}
  virtual void SendErrorNotFound(uint32 stream_id) {}
  virtual void SendOKResponse(uint32 stream_id, std::string output) {}
  virtual size_t SendSynStream(uint32 stream_id, const BalsaHeaders& headers);
  virtual size_t SendSynReply(uint32 stream_id, const BalsaHeaders& headers);
  virtual void SendDataFrame(uint32 stream_id, const char* data, int64 len,
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
  virtual void GetOutput() {}

  SMConnection* connection_;
  SMInterface* sm_other_interface_;
  EpollServer* epoll_server_;
  FlipAcceptor* acceptor_;
};

}  // namespace net

#endif  // NET_TOOLS_FLIP_SERVER_STREAMER_INTERFACE_

