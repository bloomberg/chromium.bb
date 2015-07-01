// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_platform_key.h"

#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "net/ssl/ssl_private_key.h"

namespace net {

scoped_ptr<SSLPrivateKey> FetchClientCertPrivateKey(
    X509Certificate* certificate,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  // TODO(davidben): Implement client auth for NSS. https://crbug.com/479036
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace net
