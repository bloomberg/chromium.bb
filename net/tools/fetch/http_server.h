// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_TOOLS_HTTP_SERVER_H_
#define NET_BASE_TOOLS_HTTP_SERVER_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "net/tools/fetch/http_session.h"

// Implements a simple, single-threaded HttpServer.
// Right now, this class implements no functionality above and beyond
// the HttpSession.
class HttpServer {
public:
  HttpServer(std::string ip, int port);
  ~HttpServer();

private:
  scoped_ptr<HttpSession> session_;
  DISALLOW_COPY_AND_ASSIGN(HttpServer);
};

#endif // NET_BASE_TOOLS_HTTP_SERVER_H_

