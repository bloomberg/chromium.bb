// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/protobuf_video_reader.h"

#include "base/bind.h"
#include "base/task.h"
#include "net/socket/stream_socket.h"
#include "remoting/base/constants.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/session.h"

namespace remoting {
namespace protocol {

ProtobufVideoReader::ProtobufVideoReader(VideoPacketFormat::Encoding encoding)
    : session_(NULL),
      encoding_(encoding),
      video_stub_(NULL) {
}

ProtobufVideoReader::~ProtobufVideoReader() {
  if (session_)
    session_->CancelChannelCreation(kVideoChannelName);
}

void ProtobufVideoReader::Init(protocol::Session* session,
                               VideoStub* video_stub,
                               const InitializedCallback& callback) {
  session_ = session;
  initialized_callback_ = callback;
  video_stub_ = video_stub;

  session_->CreateStreamChannel(
      kVideoChannelName,
      base::Bind(&ProtobufVideoReader::OnChannelReady, base::Unretained(this)));
}

void ProtobufVideoReader::OnChannelReady(net::StreamSocket* socket) {
  if (!socket) {
    initialized_callback_.Run(false);
    return;
  }

  DCHECK(!channel_.get());
  channel_.reset(socket);
  reader_.Init(socket, base::Bind(&ProtobufVideoReader::OnNewData,
                                  base::Unretained(this)));
  initialized_callback_.Run(true);
}

void ProtobufVideoReader::OnNewData(VideoPacket* packet,
                                    const base::Closure& done_task) {
  video_stub_->ProcessVideoPacket(packet, done_task);
}

}  // namespace protocol
}  // namespace remoting
