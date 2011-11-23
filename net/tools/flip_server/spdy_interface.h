// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_FLIP_SERVER_SPDY_INTERFACE_H_
#define NET_TOOLS_FLIP_SERVER_SPDY_INTERFACE_H_

#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_protocol.h"
#include "net/tools/flip_server/balsa_headers.h"
#include "net/tools/flip_server/balsa_visitor_interface.h"
#include "net/tools/flip_server/output_ordering.h"
#include "net/tools/flip_server/sm_connection.h"
#include "net/tools/flip_server/sm_interface.h"

namespace net {

class FlipAcceptor;
class MemoryCache;

class SpdySM : public spdy::SpdyFramerVisitorInterface,
               public SMInterface {
 public:
  SpdySM(SMConnection* connection,
         SMInterface* sm_http_interface,
         EpollServer* epoll_server,
         MemoryCache* memory_cache,
         FlipAcceptor* acceptor);
  virtual ~SpdySM();

  virtual void InitSMInterface(SMInterface* sm_http_interface,
                               int32 server_idx) OVERRIDE {}

  virtual void InitSMConnection(SMConnectionPoolInterface* connection_pool,
                                SMInterface* sm_interface,
                                EpollServer* epoll_server,
                                int fd,
                                std::string server_ip,
                                std::string server_port,
                                std::string remote_ip,
                                bool use_ssl) OVERRIDE;

  static bool disable_data_compression() { return disable_data_compression_; }
  static void set_disable_data_compression(bool value) {
    disable_data_compression_ = value;
  }

 private:
  virtual void set_is_request() OVERRIDE {}
  virtual void OnError(spdy::SpdyFramer* framer) OVERRIDE {}
  SMInterface* NewConnectionInterface();
  SMInterface* FindOrMakeNewSMConnectionInterface(std::string server_ip,
                                                  std::string server_port);
  int SpdyHandleNewStream(const spdy::SpdyControlFrame* frame,
                          std::string& http_data,
                          bool* is_https_scheme);

  // SpdyFramerVisitor interface.
  virtual void OnControl(const spdy::SpdyControlFrame* frame) OVERRIDE;
  virtual bool OnControlFrameHeaderData(spdy::SpdyStreamId stream_id,
                                        const char* header_data,
                                        size_t len) OVERRIDE;
  virtual void OnDataFrameHeader(const spdy::SpdyDataFrame* frame) OVERRIDE;
  virtual void OnStreamFrameData(spdy::SpdyStreamId stream_id,
                                 const char* data, size_t len) OVERRIDE;

 public:
  virtual size_t ProcessReadInput(const char* data, size_t len) OVERRIDE;
  virtual size_t ProcessWriteInput(const char* data, size_t len) OVERRIDE;
  virtual bool MessageFullyRead() const OVERRIDE;
  virtual void SetStreamID(uint32 stream_id) OVERRIDE {}
  virtual bool Error() const OVERRIDE;
  virtual const char* ErrorAsString() const OVERRIDE;
  virtual void Reset() OVERRIDE {}
  virtual void ResetForNewInterface(int32 server_idx) OVERRIDE;
  virtual void ResetForNewConnection() OVERRIDE;
  // SMInterface's Cleanup is currently only called by SMConnection after a
  // protocol message as been fully read. Spdy's SMInterface does not need
  // to do any cleanup at this time.
  // TODO(klindsay) This method is probably not being used properly and
  // some logic review and method renaming is probably in order.
  virtual void Cleanup() OVERRIDE {}
  // Send a settings frame
  virtual int PostAcceptHook() OVERRIDE;
  virtual void NewStream(uint32 stream_id,
                         uint32 priority,
                         const std::string& filename) OVERRIDE;
  void AddToOutputOrder(const MemCacheIter& mci);
  virtual void SendEOF(uint32 stream_id) OVERRIDE;
  virtual void SendErrorNotFound(uint32 stream_id) OVERRIDE;
  void SendOKResponse(uint32 stream_id, std::string* output);
  virtual size_t SendSynStream(uint32 stream_id,
                               const BalsaHeaders& headers) OVERRIDE;
  virtual size_t SendSynReply(uint32 stream_id,
                              const BalsaHeaders& headers) OVERRIDE;
  virtual void SendDataFrame(uint32 stream_id, const char* data, int64 len,
                             uint32 flags, bool compress) OVERRIDE;
  spdy::SpdyFramer* spdy_framer() { return spdy_framer_; }

  static std::string forward_ip_header() { return forward_ip_header_; }
  static void set_forward_ip_header(std::string value) {
    forward_ip_header_ = value;
  }

 private:
  void SendEOFImpl(uint32 stream_id);
  void SendErrorNotFoundImpl(uint32 stream_id);
  void SendOKResponseImpl(uint32 stream_id, std::string* output);
  void KillStream(uint32 stream_id);
  void CopyHeaders(spdy::SpdyHeaderBlock& dest, const BalsaHeaders& headers);
  size_t SendSynStreamImpl(uint32 stream_id, const BalsaHeaders& headers);
  size_t SendSynReplyImpl(uint32 stream_id, const BalsaHeaders& headers);
  void SendDataFrameImpl(uint32 stream_id, const char* data, int64 len,
                         spdy::SpdyDataFlags flags, bool compress);
  void EnqueueDataFrame(DataFrame* df);
  virtual void GetOutput() OVERRIDE;
 private:
  uint64 seq_num_;
  spdy::SpdyFramer* spdy_framer_;
  bool valid_spdy_session_;  // True if we have seen valid data on this session.
                             // Use this to fail fast when junk is sent to our
                             // port.

  SMConnection* connection_;
  OutputList* client_output_list_;
  OutputOrdering client_output_ordering_;
  uint32 next_outgoing_stream_id_;
  EpollServer* epoll_server_;
  FlipAcceptor* acceptor_;
  MemoryCache* memory_cache_;
  std::vector<SMInterface*> server_interface_list;
  std::vector<int32> unused_server_interface_list;
  typedef std::map<uint32, SMInterface*> StreamToSmif;
  StreamToSmif stream_to_smif_;
  bool close_on_error_;

  static bool disable_data_compression_;
  static std::string forward_ip_header_;
};

}  // namespace net

#endif  // NET_TOOLS_FLIP_SERVER_SPDY_INTERFACE_H_
