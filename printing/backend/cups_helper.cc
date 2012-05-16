// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/cups_helper.h"

#include "base/logging.h"
#include "googleurl/src/gurl.h"

namespace printing {

// Default port for IPP print servers.
static const int kDefaultIPPServerPort = 631;

// Helper wrapper around http_t structure, with connection and cleanup
// functionality.
HttpConnectionCUPS::HttpConnectionCUPS(const GURL& print_server_url,
                                       http_encryption_t encryption)
    : http_(NULL) {
  // If we have an empty url, use default print server.
  if (print_server_url.is_empty())
    return;

  int port = print_server_url.IntPort();
  if (port == url_parse::PORT_UNSPECIFIED)
    port = kDefaultIPPServerPort;

  http_ = httpConnectEncrypt(print_server_url.host().c_str(), port,
                             encryption);
  if (http_ == NULL) {
    LOG(ERROR) << "CP_CUPS: Failed connecting to print server: " <<
               print_server_url;
  }
}

HttpConnectionCUPS::~HttpConnectionCUPS() {
  if (http_ != NULL)
    httpClose(http_);
}

void HttpConnectionCUPS::SetBlocking(bool blocking) {
  httpBlocking(http_, blocking ?  1 : 0);
}

http_t* HttpConnectionCUPS::http() {
  return http_;
}

}  // namespace printing
