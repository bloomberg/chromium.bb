// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_SSL_CONTEXT_H_
#define PLATFORM_IMPL_SSL_CONTEXT_H_

#include <memory>

#include "absl/strings/string_view.h"
#include "openssl/base.h"
#include "openssl/ssl.h"

namespace openscreen {
template <typename Value>
class ErrorOr;

namespace platform {

// This class abstracts out OpenSSL's concept of an SSL Context, taking the
// responsibility of generating new TLS connection objects.
class SSLContext {
 public:
  static ErrorOr<SSLContext> Create(absl::string_view cert_filename,
                                    absl::string_view key_filename);

  SSLContext() = default;
  SSLContext(SSLContext&&) = default;
  virtual ~SSLContext() = default;
  SSLContext& operator=(SSLContext&&) = default;

  // An |SSL| object represents a single TLS or DTLS connection. Although the
  // shared |SSL_CTX| is thread-safe, an |SSL| is not thread-safe and may only
  // be used on one thread at a time. Calling classes can use the returned SSL
  // to open a TLS Socket.
  bssl::UniquePtr<SSL> CreateSSL() const;

 private:
  bssl::UniquePtr<SSL_CTX> context_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_SSL_CONTEXT_H_
