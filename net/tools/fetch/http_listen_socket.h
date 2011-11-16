// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_TOOLS_HTTP_LISTEN_SOCKET_H_
#define NET_BASE_TOOLS_HTTP_LISTEN_SOCKET_H_
#pragma once

#include "base/message_loop.h"
#include "net/base/listen_socket.h"

class HttpServerRequestInfo;
class HttpServerResponseInfo;

// Implements a simple HTTP listen socket on top of the raw socket interface.
class HttpListenSocket : public net::ListenSocket,
                         public net::ListenSocket::ListenSocketDelegate {
 public:
  class Delegate {
   public:
    virtual void OnRequest(HttpListenSocket* connection,
                           HttpServerRequestInfo* info) = 0;

   protected:
    virtual ~Delegate() {}
  };

  static HttpListenSocket* Listen(const std::string& ip, int port,
                                  HttpListenSocket::Delegate* delegate);

  virtual void Listen() OVERRIDE;
  virtual void Accept() OVERRIDE;

  // Send a server response.
  // TODO(mbelshe): make this capable of non-ascii data.
  void Respond(HttpServerResponseInfo* info, std::string& data);

  // ListenSocketDelegate
  virtual void DidAccept(net::ListenSocket* server,
                         net::ListenSocket* connection) OVERRIDE;
  virtual void DidRead(net::ListenSocket* connection,
                       const char* data, int len) OVERRIDE;
  virtual void DidClose(net::ListenSocket* sock) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<net::ListenSocket>;

  static const int kReadBufSize = 16 * 1024;
  HttpListenSocket(SOCKET s, HttpListenSocket::Delegate* del);
  virtual ~HttpListenSocket();

  // Expects the raw data to be stored in recv_data_. If parsing is successful,
  // will remove the data parsed from recv_data_, leaving only the unused
  // recv data.
  HttpServerRequestInfo* ParseHeaders();

  HttpListenSocket::Delegate* delegate_;
  std::string recv_data_;

  DISALLOW_COPY_AND_ASSIGN(HttpListenSocket);
};

#endif // NET_BASE_TOOLS_HTTP_LISTEN_SOCKET_H_
