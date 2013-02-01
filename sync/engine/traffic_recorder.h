// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ENGINE_TRAFFIC_RECORDER_H_
#define CHROME_BROWSER_SYNC_ENGINE_TRAFFIC_RECORDER_H_

#include <deque>
#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/values.h"
#include "sync/base/sync_export.h"
#include "sync/protocol/sync.pb.h"

namespace sync_pb {
class ClientToServerResponse;
class ClientToServerMessage;
}

namespace syncer {

class SYNC_EXPORT_PRIVATE TrafficRecorder {
 public:
  enum TrafficMessageType {
    CLIENT_TO_SERVER_MESSAGE,
    CLIENT_TO_SERVER_RESPONSE,
    UNKNOWN_MESSAGE_TYPE
  };

  struct SYNC_EXPORT_PRIVATE TrafficRecord {
    // The serialized message.
    std::string message;
    TrafficMessageType message_type;
    // If the message is too big to be kept in memory then it should be
    // truncated. For now the entire message is omitted if it is too big.
    // TODO(lipalani): Truncate the specifics to fit within size.
    bool truncated;

    TrafficRecord(const std::string& message,
                  TrafficMessageType message_type,
                  bool truncated);
    TrafficRecord();
    ~TrafficRecord();
    DictionaryValue* ToValue() const;
  };

  TrafficRecorder(unsigned int max_messages, unsigned int max_message_size);
  ~TrafficRecorder();

  void RecordClientToServerMessage(const sync_pb::ClientToServerMessage& msg);
  void RecordClientToServerResponse(
      const sync_pb::ClientToServerResponse& response);
  ListValue* ToValue() const;

  const std::deque<TrafficRecord>& records() {
    return records_;
  }

 private:
  void AddTrafficToQueue(TrafficRecord* record);
  void StoreProtoInQueue(const ::google::protobuf::MessageLite& msg,
                         TrafficMessageType type);

  // Maximum number of messages stored in the queue.
  unsigned int max_messages_;

  // Maximum size of each message.
  unsigned int max_message_size_;
  std::deque<TrafficRecord> records_;
  DISALLOW_COPY_AND_ASSIGN(TrafficRecorder);
};

}  // namespace syncer

#endif  // CHROME_BROWSER_SYNC_ENGINE_TRAFFIC_RECORDER_H_

