// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/string_number_conversions.h"
#include "remoting/base/constants.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/fake_session.h"
#include "remoting/protocol/rtp_reader.h"
#include "remoting/protocol/rtp_utils.h"
#include "remoting/protocol/rtp_video_writer.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::IOBuffer;
using std::string;
using std::vector;

namespace remoting {
namespace protocol {

namespace {

bool ParsePacket(const string& data, RtpPacket* packet) {
  int header_size = UnpackRtpHeader(
      reinterpret_cast<const uint8*>(&*data.begin()),
      data.size(), packet->mutable_header());
  if (header_size < 0) {
    return false;
  }

  int descriptor_size = UnpackVp8Descriptor(
      reinterpret_cast<const uint8*>(&*data.begin()) + header_size,
      data.size() - header_size, packet->mutable_vp8_descriptor());
  if (descriptor_size < 0) {
    return false;
  }

  packet->mutable_payload()->AppendCopyOf(
      &*data.begin() + header_size + descriptor_size,
      data.size() - header_size - descriptor_size);

  return true;
}

}  // namespace

class RtpVideoWriterTest : public testing::Test {
 protected:
  struct ExpectedPacket {
    bool first;
    Vp8Descriptor::FragmentationInfo fragmentation_info;
    bool last;
  };

  RtpVideoWriterTest()
      : writer_(base::MessageLoopProxy::CreateForCurrentThread()) {
  }

  virtual void SetUp() {
    session_.reset(new FakeSession());
    writer_.Init(session_.get(),
                 base::Bind(&RtpVideoWriterTest::OnWriterInitialized,
                            base::Unretained(this)));
  }

  void OnWriterInitialized(bool success) {
    ASSERT_TRUE(success);
  }

  void InitData(int size) {
    data_.resize(size);
    for (int i = 0; i < size; ++i) {
      data_[i] = static_cast<char>(i);
    }
  }

  void InitPacket(int size, bool first, bool last) {
    InitData(size);

    packet_ = new VideoPacket();
    packet_->mutable_format()->set_encoding(VideoPacketFormat::ENCODING_VP8);
    if (first)
      packet_->set_flags(packet_->flags() | VideoPacket::FIRST_PACKET);
    if (last)
      packet_->set_flags(packet_->flags() | VideoPacket::LAST_PACKET);
    packet_->mutable_data()->assign(data_.begin(), data_.end());
  }

  bool CompareData(const CompoundBuffer& buffer, char* data, int size) {
    scoped_refptr<IOBuffer> buffer_data = buffer.ToIOBufferWithSize();
    return buffer.total_bytes() == size &&
        memcmp(buffer_data->data(), data, size) == 0;
  }

  void VerifyResult(const ExpectedPacket expected[],
                    int count) {
    const vector<string>& rtp_packets =
        session_->GetDatagramChannel(kVideoRtpChannelName)->written_packets();
    ASSERT_EQ(count, static_cast<int>(rtp_packets.size()));
    int pos = 0;
    for (int i = 0; i < count; ++i) {
      SCOPED_TRACE("Packet " + base::IntToString(i));

      RtpPacket packet;
      ASSERT_TRUE(ParsePacket(rtp_packets[i], &packet));
      EXPECT_EQ(expected[i].first, packet.vp8_descriptor().frame_beginning);
      EXPECT_EQ(expected[i].last, packet.header().marker);
      EXPECT_EQ(expected[i].fragmentation_info,
                packet.vp8_descriptor().fragmentation_info);
      EXPECT_TRUE(CompareData(packet.payload(), &*data_.begin() + pos,
                              packet.payload().total_bytes()));
      pos += packet.payload().total_bytes();
    }
    EXPECT_EQ(pos, static_cast<int>(data_.size()));
  }

  MessageLoop message_loop_;

  scoped_ptr<FakeSession> session_;
  RtpVideoWriter writer_;

  vector<char> data_;
  VideoPacket* packet_;
};

TEST_F(RtpVideoWriterTest, NotFragmented_FirstPacket) {
  InitPacket(1024, true, false);
  writer_.ProcessVideoPacket(packet_, new DeleteTask<VideoPacket>(packet_));
  message_loop_.RunAllPending();

  ExpectedPacket expected[] = {
    { true, Vp8Descriptor::NOT_FRAGMENTED, false }
  };
  VerifyResult(expected, arraysize(expected));
}

TEST_F(RtpVideoWriterTest, NotFragmented_LastPackes) {
  InitPacket(1024, false, true);
  writer_.ProcessVideoPacket(packet_, new DeleteTask<VideoPacket>(packet_));
  message_loop_.RunAllPending();

  ExpectedPacket expected[] = {
    { false, Vp8Descriptor::NOT_FRAGMENTED, true }
  };
  VerifyResult(expected, arraysize(expected));
}

TEST_F(RtpVideoWriterTest, TwoFragments_FirstPacket) {
  InitPacket(2000, true, false);
  writer_.ProcessVideoPacket(packet_, new DeleteTask<VideoPacket>(packet_));
  message_loop_.RunAllPending();

  ExpectedPacket expected[] = {
    { true, Vp8Descriptor::FIRST_FRAGMENT, false },
    { false, Vp8Descriptor::LAST_FRAGMENT, false },
  };
  VerifyResult(expected, arraysize(expected));
}

TEST_F(RtpVideoWriterTest, TwoFragments_LastPacket) {
  InitPacket(2000, false, true);
  writer_.ProcessVideoPacket(packet_, new DeleteTask<VideoPacket>(packet_));
  message_loop_.RunAllPending();

  ExpectedPacket expected[] = {
    { false, Vp8Descriptor::FIRST_FRAGMENT, false },
    { false, Vp8Descriptor::LAST_FRAGMENT, true },
  };
  VerifyResult(expected, arraysize(expected));
}

TEST_F(RtpVideoWriterTest, ThreeFragments) {
  InitPacket(3000, true, true);
  writer_.ProcessVideoPacket(packet_, new DeleteTask<VideoPacket>(packet_));
  message_loop_.RunAllPending();

  ExpectedPacket expected[] = {
    { true, Vp8Descriptor::FIRST_FRAGMENT, false },
    { false, Vp8Descriptor::MIDDLE_FRAGMENT, false },
    { false, Vp8Descriptor::LAST_FRAGMENT, true },
  };
  VerifyResult(expected, arraysize(expected));
}

}  // namespace protocol
}  // namespace remoting
