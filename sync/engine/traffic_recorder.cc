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
#include "sync/util/time.h"

namespace syncer {

// Return current time.
base::Time TrafficRecorder::GetTime() {
  return base::Time::Now();
}

TrafficRecorder::TrafficRecord::TrafficRecord(const std::string& message,
                                              TrafficMessageType message_type,
                                              bool truncated,
                                              base::Time time) :
    message(message),
    message_type(message_type),
    truncated(truncated),
    timestamp(time) {
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

namespace {
const char* GetMessageTypeString(TrafficRecorder::TrafficMessageType type) {
  switch(type) {
    case TrafficRecorder::CLIENT_TO_SERVER_MESSAGE:
      return "Request";
    case TrafficRecorder::CLIENT_TO_SERVER_RESPONSE:
      return "Response";
    default:
      NOTREACHED();
      return "";
  }
}
}

DictionaryValue* TrafficRecorder::TrafficRecord::ToValue() const {
  scoped_ptr<DictionaryValue> value;
  if (truncated) {
    value.reset(new DictionaryValue());
    value->SetString("message_type",
                     GetMessageTypeString(message_type));
    value->SetBoolean("truncated", true);
  } else if (message_type == TrafficRecorder::CLIENT_TO_SERVER_MESSAGE) {
    sync_pb::ClientToServerMessage message_proto;
    if (message_proto.ParseFromString(message))
      value.reset(
          ClientToServerMessageToValue(message_proto,
                                       false /* include_specifics */));
  } else if (message_type == TrafficRecorder::CLIENT_TO_SERVER_RESPONSE) {
    sync_pb::ClientToServerResponse message_proto;
    if (message_proto.ParseFromString(message))
      value.reset(
          ClientToServerResponseToValue(message_proto,
                                        false /* include_specifics */));
  } else {
    NOTREACHED();
  }

  value->SetString("timestamp", GetTimeDebugString(timestamp));

  return value.release();
}


ListValue* TrafficRecorder::ToValue() const {
  scoped_ptr<ListValue> value(new ListValue());
  std::deque<TrafficRecord>::const_iterator it;
  for (it = records_.begin(); it != records_.end(); ++it) {
    const TrafficRecord& record = *it;
    value->Append(record.ToValue());
  }

  return value.release();
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

  TrafficRecord record(message, type, truncated, GetTime());
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

}  // namespace syncer

