// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/native_messaging/native_messaging_pipe.h"

#include <utility>

#include "base/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"

namespace remoting {

NativeMessagingPipe::NativeMessagingPipe() = default;
NativeMessagingPipe::~NativeMessagingPipe() = default;

void NativeMessagingPipe::Start(
    std::unique_ptr<extensions::NativeMessageHost> host,
    std::unique_ptr<extensions::NativeMessagingChannel> channel) {
  host_ = std::move(host);
  channel_ = std::move(channel);
  channel_->Start(this);
}

void NativeMessagingPipe::OnMessage(std::unique_ptr<base::Value> message) {
  std::string message_json;
  base::JSONWriter::Write(*message, &message_json);
  host_->OnMessage(message_json);
}

void NativeMessagingPipe::OnDisconnect() {
  host_.reset();
  channel_.reset();
}

void NativeMessagingPipe::PostMessageFromNativeHost(
    const std::string& message) {
  std::unique_ptr<base::Value> json = base::JSONReader::ReadDeprecated(message);
  channel_->SendMessage(std::move(json));
}

void NativeMessagingPipe::CloseChannel(const std::string& error_message) {
  host_.reset();
  channel_.reset();
}

}  // namespace remoting
