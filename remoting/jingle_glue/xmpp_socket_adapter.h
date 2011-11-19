// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_JINGLE_GLUE_XMPP_SOCKET_ADAPTER_H_
#define REMOTING_JINGLE_GLUE_XMPP_SOCKET_ADAPTER_H_

#include <string>

#include "third_party/libjingle/source/talk/base/asyncsocket.h"
#include "third_party/libjingle/source/talk/xmpp/asyncsocket.h"
#include "third_party/libjingle/source/talk/xmpp/xmppclientsettings.h"
#include "third_party/libjingle/source/talk/xmpp/xmppengine.h"

#ifndef _WIN32
// Additional errors used by us from Win32 headers.
#define SEC_E_CERT_EXPIRED static_cast<int>(0x80090328L)
#define WSA_NOT_ENOUGH_MEMORY ENOMEM
#endif

namespace remoting {

class XmppSocketAdapter : public buzz::AsyncSocket,
                          public sigslot::has_slots<> {
 public:
  XmppSocketAdapter(const buzz::XmppClientSettings& xcs,
                    bool allow_unverified_certs);
  virtual ~XmppSocketAdapter();

  virtual State state() OVERRIDE;
  virtual Error error() OVERRIDE;
  virtual int GetError() OVERRIDE;

  void set_firewall(bool firewall) { firewall_ = firewall; }

  virtual bool Connect(const talk_base::SocketAddress& addr) OVERRIDE;
  virtual bool Read(char* data, size_t len, size_t* len_read) OVERRIDE;
  virtual bool Write(const char* data, size_t len) OVERRIDE;
  virtual bool Close() OVERRIDE;

#if defined(FEATURE_ENABLE_SSL)
  virtual bool StartTls(const std::string& domainname) OVERRIDE;
  bool IsOpen() const { return state_ == STATE_OPEN
                            || state_ == STATE_TLS_OPEN; }
#else
  bool IsOpen() const { return state_ == STATE_OPEN; }
#endif

  sigslot::signal0<> SignalAuthenticationError;

 private:
  // Return false if the socket is closed.
  bool HandleReadable();
  bool HandleWritable();

  State state_;
  Error error_;
  int wsa_error_;

  talk_base::AsyncSocket* socket_;
  cricket::ProtocolType protocol_;
  talk_base::ProxyInfo proxy_;
  bool firewall_;
  char* write_buffer_;
  size_t write_buffer_length_;
  size_t write_buffer_capacity_;
  bool allow_unverified_certs_;

  bool FreeState();
  void NotifyClose();

  void OnReadEvent(talk_base::AsyncSocket* socket);
  void OnWriteEvent(talk_base::AsyncSocket* socket);
  void OnConnectEvent(talk_base::AsyncSocket* socket);
  void OnCloseEvent(talk_base::AsyncSocket* socket, int error);

  void QueueWriteData(const char* data, size_t len);
  void FlushWriteQueue(Error* error, int* wsa_error);

  void SetError(Error error);
  void SetWSAError(int error);
  DISALLOW_COPY_AND_ASSIGN(XmppSocketAdapter);
};

}  // namespace remoting

#endif  // REMOTING_JINGLE_GLUE_XMPP_SOCKET_ADAPTER_H_
