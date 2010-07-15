// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CLIENT_CONNECTION_H_
#define REMOTING_HOST_CLIENT_CONNECTION_H_

#include <deque>
#include <vector>

#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "remoting/base/protocol_decoder.h"
#include "remoting/base/protocol/chromotocol.pb.h"
#include "remoting/jingle_glue/jingle_channel.h"

namespace media {

class DataBuffer;

}  // namespace media

namespace remoting {

// This class represents a remote viewer connected to the chromoting host
// through a libjingle connection. A viewer object is responsible for sending
// screen updates and other messages to the remote viewer. It is also
// responsible for receiving and parsing data from the remote viewer and
// delegating events to the event handler.
class ClientConnection : public base::RefCountedThreadSafe<ClientConnection>,
                         public JingleChannel::Callback {
 public:
  class EventHandler {
   public:
    virtual ~EventHandler() {}

    // Handles an event received by the ClientConnection. Receiver will own the
    // ClientMessages in ClientMessageList and needs to delete them.
    // Note that the sender of messages will not reference messages
    // again so it is okay to clear |messages| in this method.
    virtual void HandleMessages(ClientConnection* viewer,
                                ClientMessageList* messages) = 0;

    // Called when the network connection is opened.
    virtual void OnConnectionOpened(ClientConnection* viewer) = 0;

    // Called when the network connection is closed.
    virtual void OnConnectionClosed(ClientConnection* viewer) = 0;

    // Called when the network connection has failed.
    virtual void OnConnectionFailed(ClientConnection* viewer) = 0;
  };

  // Constructs a ClientConnection object. |message_loop| is the message loop
  // that this object runs on. A viewer object receives events and messages from
  // a libjingle channel, these events are delegated to |handler|.
  // It is guranteed that |handler| is called only on the |message_loop|.
  ClientConnection(MessageLoop* message_loop,
         ProtocolDecoder* decoder,
         EventHandler* handler);

  virtual ~ClientConnection();

  // Creates a DataBuffer object that wraps around HostMessage. The DataBuffer
  // object will be responsible for serializing and framing the message.
  // DataBuffer will also own |msg| after this call.
  static scoped_refptr<media::DataBuffer> CreateWireFormatDataBuffer(
      const HostMessage* msg);

  virtual void set_jingle_channel(JingleChannel* channel) {
    channel_ = channel;
  }

  // Returns the channel in use.
  virtual JingleChannel* jingle_channel() { return channel_; }

  // Send information to the client for initialization.
  virtual void SendInitClientMessage(int width, int height);

  // Notifies the viewer the start of an update stream.
  virtual void SendBeginUpdateStreamMessage();

  // Send encoded update stream data to the viewer.
  //
  // |data| is the actual bytes in wire format. That means it is fully framed
  // and serialized from a HostMessage. This is a special case only for
  // UpdateStreamPacket to reduce the amount of memory copies.
  //
  // |data| should be created by calling to
  // CreateWireFormatDataBuffer(HostMessage).
  virtual void SendUpdateStreamPacketMessage(
      scoped_refptr<media::DataBuffer> data);

  // Notifies the viewer the update stream has ended.
  virtual void SendEndUpdateStreamMessage();

  // Gets the number of update stream messages not yet transmitted.
  // Note that the value returned is an estimate using average size of the
  // most recent update streams.
  // TODO(hclam): Report this number accurately.
  virtual int GetPendingUpdateStreamMessages();

  // Disconnect the client connection. This method is allowed to be called
  // more than once and calls after the first one will be ignored.
  //
  // After this method is called all the send method calls will be ignored.
  virtual void Disconnect();

  /////////////////////////////////////////////////////////////////////////////
  // JingleChannel::Callback implmentations
  virtual void OnStateChange(JingleChannel* channel,
                             JingleChannel::State state);
  virtual void OnPacketReceived(JingleChannel* channel,
                                scoped_refptr<media::DataBuffer> data);

 protected:
  // Protected constructor used by unit test.
  ClientConnection() {}

 private:
  // Process a libjingle state change event on the |loop_|.
  void StateChangeTask(JingleChannel::State state);

  // Process a data buffer received from libjingle.
  void PacketReceivedTask(scoped_refptr<media::DataBuffer> data);

  // The libjingle channel used to send and receive data from the remote viewer.
  scoped_refptr<JingleChannel> channel_;

  // The message loop that this object runs on.
  MessageLoop* loop_;

  // An object used by the ClientConnection to decode data received from the
  // network.
  scoped_ptr<ProtocolDecoder> decoder_;

  // A queue to count the sizes of the last 10 update streams.
  std::deque<int> size_queue_;

  // Count the sum of sizes in the queue.
  int size_in_queue_;

  // Measure the number of bytes of the current upstream stream.
  int update_stream_size_;

  // Event handler for handling events sent from this object.
  EventHandler* handler_;

  DISALLOW_COPY_AND_ASSIGN(ClientConnection);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CLIENT_CONNECTION_H_
