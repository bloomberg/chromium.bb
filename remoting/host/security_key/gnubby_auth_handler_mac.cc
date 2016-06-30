// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/gnubby_auth_handler.h"

#include "base/logging.h"

namespace remoting {

std::unique_ptr<GnubbyAuthHandler> GnubbyAuthHandler::Create(
    ClientSessionDetails* client_session_details,
    const SendMessageCallback& send_message_callback) {
  return nullptr;
}

}  // namespace remoting
