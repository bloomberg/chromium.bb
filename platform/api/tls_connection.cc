// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/tls_connection.h"

#include <cassert>

namespace openscreen {

TlsConnection::TlsConnection() = default;
TlsConnection::~TlsConnection() = default;

// TODO(crbug/openscreen/80): Remove after Chromium is migrated to Send().
void TlsConnection::Client::OnWriteBlocked(TlsConnection* connection) {
  assert(false);  // Not reached - Open Screen code does not call this anymore.
}

// TODO(crbug/openscreen/80): Remove after Chromium is migrated to Send().
void TlsConnection::Client::OnWriteUnblocked(TlsConnection* connection) {
  assert(false);  // Not reached - Open Screen code does not call this anymore.
}

// TODO(crbug/openscreen/80): Remove after Chromium implements Send().
bool TlsConnection::Send(const void* data, size_t len) {
  Write(data, len);  // Note: Will call subclass's override for Write().
  return true;
}

// TODO(crbug/openscreen/80): Remove after Chromium is migrated to Send().
void TlsConnection::Write(const void* data, size_t len) {
  assert(false);  // Not reached - Open Screen code does not call this anymore.
}

}  // namespace openscreen
