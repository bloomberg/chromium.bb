// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp_base/boringssl_util.h"

#include "absl/strings/string_view.h"
#include "openssl/err.h"
#include "openssl/ssl.h"
#include "platform/api/logging.h"

namespace openscreen {

namespace {

int BoringSslErrorCallback(const char* str, size_t len, void* context) {
  OSP_LOG_ERROR << "[BoringSSL] " << absl::string_view(str, len);
  return 1;
}

}  // namespace

void LogAndClearBoringSslErrors() {
  ERR_print_errors_cb(BoringSslErrorCallback, nullptr);
  ERR_clear_error();
}

// Multiple sequential calls to InitOpenSSL or CleanupOpenSSL are ignored
// by OpenSSL itself.
void InitOpenSSL() {
  OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS, nullptr);
}

void CleanupOpenSSL() {
  EVP_cleanup();
}

}  // namespace openscreen
