// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_PUBLIC_MESSAGE_DEMUXER_H_
#define API_PUBLIC_MESSAGE_DEMUXER_H_

#include <map>
#include <memory>
#include <vector>

#include "api/public/clock.h"
#include "base/error.h"
#include "msgs/osp_messages.h"
#include "platform/api/time.h"

namespace openscreen {

class QuicStream;

// This class separates QUIC stream data into CBOR messages by reading a type
// prefix from the stream and passes those messages to any callback matching the
// source endpoint and message type.  If there is no callback for a given
// message type, it will also try a default message listener.
class MessageDemuxer {
 public:
  class MessageCallback {
   public:
    virtual ~MessageCallback() = default;

    // |buffer| contains data for a message of type |message_type|.  However,
    // the data may be incomplete, in which case the callback should return an
    // error code of Error::Code::kCborIncompleteMessage.  This way, the
    // MessageDemuxer knows to neither consume the data nor discard it as bad.
    virtual ErrorOr<size_t> OnStreamMessage(uint64_t endpoint_id,
                                            uint64_t connection_id,
                                            msgs::Type message_type,
                                            const uint8_t* buffer,
                                            size_t buffer_size,
                                            platform::TimeDelta now) = 0;
  };

  class MessageWatch {
   public:
    MessageWatch();
    MessageWatch(MessageDemuxer* parent,
                 bool is_default,
                 uint64_t endpoint_id,
                 msgs::Type message_type);
    MessageWatch(MessageWatch&&);
    ~MessageWatch();
    MessageWatch& operator=(MessageWatch&&);

    explicit operator bool() const { return parent_; }

   private:
    MessageDemuxer* parent_ = nullptr;
    bool is_default_;
    uint64_t endpoint_id_;
    msgs::Type message_type_;
  };

  static constexpr size_t kDefaultBufferLimit = 1 << 16;

  explicit MessageDemuxer(size_t buffer_limit = kDefaultBufferLimit,
                          std::unique_ptr<Clock> clock = nullptr);
  ~MessageDemuxer();

  // Starts watching for messages of type |message_type| from the endpoint
  // identified by |endpoint_id|.  When such a message arrives, or if some are
  // already buffered, |callback| will be called with the message data.
  MessageWatch WatchMessageType(uint64_t endpoint_id,
                                msgs::Type message_type,
                                MessageCallback* callback);

  // Starts watching for messages of type |message_type| from any endpoint when
  // there is not callback set for its specific endpoint ID.
  MessageWatch SetDefaultMessageTypeWatch(msgs::Type message_type,
                                          MessageCallback* callback);

  // Gives data from |endpoint_id| to the demuxer for processing.
  // TODO(btolsch): It'd be nice if errors could propagate out of here to close
  // the stream.
  void OnStreamData(uint64_t endpoint_id,
                    uint64_t connection_id,
                    const uint8_t* data,
                    size_t data_size);

 private:
  struct HandleStreamBufferResult {
    bool handled;
    size_t consumed;
  };

  void StopWatchingMessageType(uint64_t endpoint_id, msgs::Type message_type);
  void StopDefaultMessageTypeWatch(msgs::Type message_type);

  HandleStreamBufferResult HandleStreamBufferLoop(
      uint64_t endpoint_id,
      uint64_t connection_id,
      std::map<uint64_t, std::map<msgs::Type, MessageCallback*>>::iterator
          endpoint_entry,
      std::vector<uint8_t>* buffer);

  HandleStreamBufferResult HandleStreamBuffer(
      uint64_t endpoint_id,
      uint64_t connection_id,
      std::map<msgs::Type, MessageCallback*>* message_callbacks,
      std::vector<uint8_t>* buffer);

  const size_t buffer_limit_;
  std::map<uint64_t, std::map<msgs::Type, MessageCallback*>> message_callbacks_;
  std::map<msgs::Type, MessageCallback*> default_callbacks_;
  std::map<uint64_t, std::map<uint64_t, std::vector<uint8_t>>> buffers_;
  std::unique_ptr<Clock> clock_;
};

// TODO(btolsch): Make sure all uses of MessageWatch are converted to this
// resest function for readability.
void StopWatching(MessageDemuxer::MessageWatch* watch);

}  // namespace openscreen

#endif  // API_PUBLIC_MESSAGE_DEMUXER_H_
