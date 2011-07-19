// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/fetch/http_server_response_info.h"

HttpServerResponseInfo::HttpServerResponseInfo()
    : status(200), content_length(0), connection_close(false) {
}

HttpServerResponseInfo::~HttpServerResponseInfo() {}
