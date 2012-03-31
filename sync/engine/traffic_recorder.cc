// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/traffic_recorder.h"

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "sync/protocol/proto_value_conversions.h"
#include "sync/protocol/sync.pb.h"
#include "sync/sessions/sync_session.h"

namespace browser_sync {

TrafficRecorder::TrafficRecord::TrafficRecord(const std::string& message,
                                              TrafficMessageType message_type,
                                              bool truncated) :
    message(message),
    message_type(message_type),
    truncated(truncated) {
}

TrafficRecorder::TrafficRecord::TrafficRecord()
    : message_type(UNKNOWN_MESSAGE_TYPE),
      truncated(false) {
}

TrafficRecorder::TrafficRecord::~TrafficRecord() {
}

TrafficRecorder::TrafficRecorder(unsigned int max_messages,
    unsigned int max_message_size)
    : max_messages_(max_messages),
      max_message_size_(max_message_size) {
}

TrafficRecorder::~TrafficRecorder() {
}


void TrafficRecorder::AddTrafficToQueue(TrafficRecord* record) {
  records_.resize(records_.size() + 1);
  std::swap(records_.back(), *record);

  // We might have more records than our limit.
  // Maintain the size invariant by deleting items.
  while (records_.size() > max_messages_) {
    records_.pop_front();
  }
}

void TrafficRecorder::StoreProtoInQueue(
    const ::google::protobuf::MessageLite& msg,
    TrafficMessageType type) {
  bool truncated = false;
  std::string message;
  if (static_cast<unsigned int>(msg.ByteSize()) >= max_message_size_) {
    // TODO(lipalani): Trim the specifics to fit in size.
    truncated = true;
  } else {
    msg.SerializeToString(&message);
  }

  TrafficRecord record(message, type, truncated);
  AddTrafficToQueue(&record);
}

void TrafficRecorder::RecordClientToServerMessage(
    const sync_pb::ClientToServerMessage& msg) {
  StoreProtoInQueue(msg, CLIENT_TO_SERVER_MESSAGE);
}

void TrafficRecorder::RecordClientToServerResponse(
    const sync_pb::ClientToServerResponse& response) {
  StoreProtoInQueue(response, CLIENT_TO_SERVER_RESPONSE);
}

}  // namespace browser_sync

