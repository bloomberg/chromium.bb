// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_status.h"

#include "net/base/net_errors.h"

namespace net {

URLRequestStatus URLRequestStatus::FromError(int error) {
  if (error == OK) {
    return URLRequestStatus(SUCCESS, OK);
  } else if (error == ERR_IO_PENDING) {
    return URLRequestStatus(IO_PENDING, ERR_IO_PENDING);
  } else if (error == ERR_ABORTED) {
    return URLRequestStatus(CANCELED, ERR_ABORTED);
  } else {
    return URLRequestStatus(FAILED, error);
  }
}

}  // namespace net
