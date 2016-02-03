// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/client_video_dispatcher.h"

#include "base/bind.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "remoting/base/buffered_socket_writer.h"
#include "remoting/base/constants.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/fake_stream_socket.h"
#include "remoting/protocol/message_reader.h"
#include "remoting/protocol/message_serialization.h"
#include "remoting/protocol/video_stub.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace protocol {

class ClientVideoDispatcherTest : public testing::Test,
                                  public VideoStub,
                                  public ChannelDispatcherBase::EventHandler {
 public:
  ClientVideoDispatcherTest();

  // VideoStub interface.
  void ProcessVideoPacket(scoped_ptr<VideoPacket> video_packet,
                          const base::Closure& done) override;

  // ChannelDispatcherBase::EventHandler interface.
  void OnChannelInitialized(ChannelDispatcherBase* channel_dispatcher) override;
  void OnChannelError(ChannelDispatcherBase* channel_dispatcher,
                      ErrorCode error) override;

 protected:
  void OnMessageReceived(scoped_ptr<CompoundBuffer> buffer);
  void OnReadError(int error);

  base::MessageLoop message_loop_;

  // Set to true in OnChannelInitialized().
  bool initialized_;

  // Client side.
  FakeStreamChannelFactory client_channel_factory_;
  ClientVideoDispatcher dispatcher_;

  // Host side.
  FakeStreamSocket host_socket_;
  MessageReader reader_;
  BufferedSocketWriter writer_;

  ScopedVector<VideoPacket> video_packets_;
  std::vector<base::Closure> packet_done_callbacks_;

  ScopedVector<VideoAck> ack_messages_;
};

ClientVideoDispatcherTest::ClientVideoDispatcherTest()
    : initialized_(false),
      dispatcher_(this) {
  dispatcher_.Init(&client_channel_factory_, this);
  base::RunLoop().RunUntilIdle();
  DCHECK(initialized_);
  host_socket_.PairWith(
      client_channel_factory_.GetFakeChannel(kVideoChannelName));
  reader_.StartReading(&host_socket_,
                       base::Bind(&ClientVideoDispatcherTest::OnMessageReceived,
                                  base::Unretained(this)),
                       base::Bind(&ClientVideoDispatcherTest::OnReadError,
                                  base::Unretained(this)));
  writer_.Start(
      base::Bind(&P2PStreamSocket::Write, base::Unretained(&host_socket_)),
      BufferedSocketWriter::WriteFailedCallback());
}

void ClientVideoDispatcherTest::ProcessVideoPacket(
    scoped_ptr<VideoPacket> video_packet,
    const base::Closure& done) {
  video_packets_.push_back(video_packet.release());
  packet_done_callbacks_.push_back(done);
}

void ClientVideoDispatcherTest::OnChannelInitialized(
    ChannelDispatcherBase* channel_dispatcher) {
  initialized_ = true;
}

void ClientVideoDispatcherTest::OnChannelError(
    ChannelDispatcherBase* channel_dispatcher,
    ErrorCode error) {
  // Don't expect channel creation to fail.
  FAIL();
}

void ClientVideoDispatcherTest::OnMessageReceived(
    scoped_ptr<CompoundBuffer> buffer) {
  scoped_ptr<VideoAck> ack = ParseMessage<VideoAck>(buffer.get());
  EXPECT_TRUE(ack);
  ack_messages_.push_back(ack.release());
}

void ClientVideoDispatcherTest::OnReadError(int error) {
  LOG(FATAL) << "Unexpected read error: " << error;
}

// Verify that the client can receive video packets and acks are not sent for
// VideoPackets that don't have frame_id field set.
TEST_F(ClientVideoDispatcherTest, WithoutAcks) {
  VideoPacket packet;
  packet.set_data(std::string());

  // Send a VideoPacket and verify that the client receives it.
  writer_.Write(SerializeAndFrameMessage(packet), base::Closure());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, video_packets_.size());

  packet_done_callbacks_.front().Run();
  base::RunLoop().RunUntilIdle();

  // Ack should never be sent for the packet without frame_id.
  EXPECT_TRUE(ack_messages_.empty());
}

// Verifies that the dispatcher sends Ack message with correct rendering delay.
TEST_F(ClientVideoDispatcherTest, WithAcks) {
  int kTestFrameId = 3;

  VideoPacket packet;
  packet.set_data(std::string());
  packet.set_frame_id(kTestFrameId);

  // Send a VideoPacket and verify that the client receives it.
  writer_.Write(SerializeAndFrameMessage(packet), base::Closure());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, video_packets_.size());

  // Ack should only be sent after the packet is processed.
  EXPECT_TRUE(ack_messages_.empty());
  base::RunLoop().RunUntilIdle();

  // Fake completion of video packet decoding, to trigger the Ack.
  packet_done_callbacks_.front().Run();
  base::RunLoop().RunUntilIdle();

  // Verify that the Ack message has been received.
  ASSERT_EQ(1U, ack_messages_.size());
  EXPECT_EQ(kTestFrameId, ack_messages_[0]->frame_id());
}

// Verify that Ack messages are sent in correct order.
TEST_F(ClientVideoDispatcherTest, AcksOrder) {
  int kTestFrameId = 3;

  VideoPacket packet;
  packet.set_data(std::string());
  packet.set_frame_id(kTestFrameId);

  // Send two VideoPackets.
  writer_.Write(SerializeAndFrameMessage(packet), base::Closure());
  base::RunLoop().RunUntilIdle();

  packet.set_frame_id(kTestFrameId + 1);
  writer_.Write(SerializeAndFrameMessage(packet), base::Closure());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2U, video_packets_.size());
  EXPECT_TRUE(ack_messages_.empty());

  // Call completion callbacks in revers order.
  packet_done_callbacks_[1].Run();
  packet_done_callbacks_[0].Run();

  base::RunLoop().RunUntilIdle();

  // Verify order of Ack messages.
  ASSERT_EQ(2U, ack_messages_.size());
  EXPECT_EQ(kTestFrameId, ack_messages_[0]->frame_id());
  EXPECT_EQ(kTestFrameId + 1, ack_messages_[1]->frame_id());
}

}  // namespace protocol
}  // namespace remoting
