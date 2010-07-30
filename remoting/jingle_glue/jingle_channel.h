// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_JINGLE_GLUE_JINGLE_CHANNEL_H_
#define REMOTING_JINGLE_GLUE_JINGLE_CHANNEL_H_

#include <deque>
#include <string>

#include "base/basictypes.h"
#include "base/condition_variable.h"
#include "base/gtest_prod_util.h"
#include "base/lock.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "third_party/libjingle/source/talk/base/sigslot.h"

namespace base {
class WaitableEvent;
}  // namespace base

namespace talk_base {
class StreamInterface;
}  // namespace talk_base

namespace media {
class Buffer;
class DataBuffer;
}  // namespace media

namespace remoting {
class JingleThread;

class JingleChannel : public base::RefCountedThreadSafe<JingleChannel> {
 public:
  enum State {
    INITIALIZING,
    CONNECTING,
    OPEN,
    CLOSED,
    FAILED,
  };

  class Callback {
   public:
    virtual ~Callback() {}

    // Called when state of the connection is changed.
    virtual void OnStateChange(JingleChannel* channel, State state) = 0;

    // Called when a new packet is received.
    virtual void OnPacketReceived(JingleChannel* channel,
                                  scoped_refptr<media::DataBuffer> data) = 0;
  };

  virtual ~JingleChannel();

  // Puts data to the write buffer.
  virtual void Write(scoped_refptr<media::DataBuffer> data);

  // Closes the tunnel.
  virtual void Close();

  // Current state of the tunnel.
  State state() const { return state_; }

  // JID of the other end of the channel.
  const std::string& jid() const { return jid_; }

  // Number of bytes currently stored in the write buffer.
  size_t write_buffer_size();

 protected:
  // Needs access to constructor, Init().
  friend class JingleClient;

  // Constructor used by unit test only.
  // TODO(hclam): Have to suppress warnings in MSVC.
  JingleChannel();

  // Used by JingleClient to create an instance of the channel. |callback|
  // must not be NULL.
  explicit JingleChannel(Callback* callback);

  // Initialized the channel. Ownership of the |stream| is transfered to
  // caller. Ownership of |thread| is not.
  void Init(JingleThread* thread, talk_base::StreamInterface* stream,
            const std::string& jid);
  void SetState(State state);

  JingleThread* thread_;
  scoped_ptr<talk_base::StreamInterface> stream_;
  State state_;

 private:
  FRIEND_TEST_ALL_PREFIXES(JingleChannelTest, Init);
  FRIEND_TEST_ALL_PREFIXES(JingleChannelTest, Write);
  FRIEND_TEST_ALL_PREFIXES(JingleChannelTest, Read);
  FRIEND_TEST_ALL_PREFIXES(JingleChannelTest, Close);

  typedef std::deque<scoped_refptr<media::DataBuffer> > DataQueue;

  // Event handler for the stream. It passes stream events from the stream
  // to JingleChannel.
  class EventHandler : public sigslot::has_slots<> {
   protected:
    explicit EventHandler(JingleChannel* channel) : channel_(channel) {}

    // Constructor used only by unit test.
    EventHandler() : channel_(NULL) {}

    void OnStreamEvent(talk_base::StreamInterface* stream,
                       int events, int error) {
      channel_->OnStreamEvent(stream, events, error);
    }
   private:
    friend class JingleChannel;
    JingleChannel* channel_;
  };
  friend class EventHandler;

  // Event handler for the stream.
  void OnStreamEvent(talk_base::StreamInterface* stream,
                     int events, int error);

  // Writes data from the buffer to the stream. Called
  // from OnStreamEvent() in the jingle thread.
  void DoWrite();

  // Reads data from the stream and puts it to the read buffer.
  // Called from OnStreamEvent() in the jingle thread.
  void DoRead();

  void DoClose(base::WaitableEvent* done_event);

  // Callback that is called on channel events. Initialized in the constructor.
  Callback* callback_;

  // Event handler for stream events.
  EventHandler event_handler_;

  // Jid of the other end of the channel.
  std::string jid_;

  // Write buffer. |write_lock_| should be locked when accessing |write_queue_|
  // and |write_buffer_size_|, but isn't necessary for |current_write_buf_|.
  // |current_write_buf_| is accessed only by the jingle thread.
  // |write_buffer_size_| stores number of bytes currently in |write_queue_|
  // and in |current_write_buf_|.
  DataQueue write_queue_;
  size_t write_buffer_size_;
  Lock write_lock_;
  scoped_refptr<media::DataBuffer> current_write_buf_;
  size_t current_write_buf_pos_;

  DISALLOW_COPY_AND_ASSIGN(JingleChannel);
};

}  // namespace remoting

#endif  // REMOTING_JINGLE_GLUE_JINGLE_CHANNEL_H_
