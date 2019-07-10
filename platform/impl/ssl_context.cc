// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/ssl_context.h"

#include <stdio.h>

#include <string>
#include <utility>

#include "openssl/err.h"
#include "platform/api/logging.h"
#include "platform/base/error.h"

namespace openscreen {
namespace platform {
namespace {

void LogLastError() {
  OSP_LOG_ERROR << ERR_error_string(ERR_get_error(), nullptr);
}

Error ConfigureContext(SSL_CTX* ctx,
                       absl::string_view cert_filename,
                       absl::string_view key_filename) {
  if (SSL_CTX_use_certificate_file(ctx, std::string(cert_filename).c_str(),
                                   SSL_FILETYPE_PEM) <= 0) {
    LogLastError();
    return Error::Code::kFileLoadFailure;
  }

  if (SSL_CTX_use_PrivateKey_file(ctx, std::string(key_filename).c_str(),
                                  SSL_FILETYPE_PEM) <= 0) {
    LogLastError();
    return Error::Code::kFileLoadFailure;
  }

  return Error::None();
}
}  // namespace

ErrorOr<SSLContext> SSLContext::Create(absl::string_view cert_filename,
                                       absl::string_view key_filename) {
  SSLContext context;
  context.context_ = bssl::UniquePtr<SSL_CTX>(SSL_CTX_new(TLS_method()));
  OSP_DCHECK(context.context_);
  Error error =
      ConfigureContext(context.context_.get(), cert_filename, key_filename);
  if (!error.ok()) {
    return error;
  }
  return context;
}

bssl::UniquePtr<SSL> SSLContext::CreateSSL() const {
  return bssl::UniquePtr<SSL>(SSL_new(context_.get()));
}
}  // namespace platform
}  // namespace openscreen
