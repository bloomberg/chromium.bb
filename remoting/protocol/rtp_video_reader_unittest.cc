// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/string_number_conversions.h"
#include "net/base/io_buffer.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/fake_session.h"
#include "remoting/protocol/rtp_utils.h"
#include "remoting/protocol/rtp_video_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::IOBuffer;
using std::vector;

namespace remoting {
namespace protocol {

class RtpVideoReaderTest : public testing::Test,
                           public VideoStub {
 public:
  // VideoStub interface.
  virtual void ProcessVideoPacket(const VideoPacket* video_packet,
                                  Task* done) {
    received_packets_.push_back(VideoPacket());
    received_packets_.back() = *video_packet;
    done->Run();
    delete done;
  }

  virtual int GetPendingPackets() {
    return 0;
  }

 protected:
  struct FragmentInfo {
    int sequence_number;
    int timestamp;
    bool first;
    bool last;
    Vp8Descriptor::FragmentationInfo fragmentation_info;
    int start;
    int end;
  };

  struct ExpectedPacket {
    int timestamp;
    int flags;
    int start;
    int end;
  };

  virtual void SetUp() {
    Reset();
    InitData(100);
  }

  virtual void TearDown() {
    message_loop_.RunAllPending();
  }

  void Reset() {
    session_.reset(new FakeSession());
    reader_.reset(new RtpVideoReader(
        base::MessageLoopProxy::current()));
    reader_->Init(session_.get(), this,
                  base::Bind(&RtpVideoReaderTest::OnReaderInitialized,
                             base::Unretained(this)));
    received_packets_.clear();
  }

  void OnReaderInitialized(bool success) {
    ASSERT_TRUE(success);
  }

  void InitData(int size) {
    data_.resize(size);
    for (int i = 0; i < size; ++i) {
      data_[i] = static_cast<char>(i);
    }
  }

  bool CompareData(const CompoundBuffer& buffer, char* data, int size) {
    scoped_refptr<IOBuffer> buffer_data = buffer.ToIOBufferWithSize();
    return buffer.total_bytes() == size &&
        memcmp(buffer_data->data(), data, size) == 0;
  }

  void SplitAndSend(const FragmentInfo fragments[], int count) {
    for (int i = 0; i < count; ++i) {
      RtpHeader header;
      header.sequence_number = fragments[i].sequence_number;
      header.marker = fragments[i].last;
      header.timestamp = fragments[i].timestamp;

      Vp8Descriptor descriptor;
      descriptor.non_reference_frame = true;
      descriptor.frame_beginning = fragments[i].first;
      descriptor.fragmentation_info = fragments[i].fragmentation_info;

      int header_size = GetRtpHeaderSize(header);
      int vp8_desc_size = GetVp8DescriptorSize(descriptor);
      int payload_size = fragments[i].end - fragments[i].start;
      int size = header_size + vp8_desc_size + payload_size;
      scoped_refptr<net::IOBuffer> buffer = new net::IOBuffer(size);

      PackRtpHeader(header, reinterpret_cast<uint8*>(buffer->data()),
                    header_size);
      PackVp8Descriptor(descriptor, reinterpret_cast<uint8*>(buffer->data()) +
                        header_size, vp8_desc_size);

      memcpy(buffer->data() + header_size + vp8_desc_size,
             &*data_.begin() + fragments[i].start, payload_size);

      reader_->rtp_reader_.OnDataReceived(buffer, size);
    }
  }

  void CheckResults(const ExpectedPacket expected[], int count) {
    ASSERT_EQ(count, static_cast<int>(received_packets_.size()));
    for (int i = 0; i < count; ++i) {
      SCOPED_TRACE("Packet " + base::IntToString(i));

      int expected_size = expected[i].end - expected[i].start;
      EXPECT_EQ(expected_size,
                static_cast<int>(received_packets_[i].data().size()));
      EXPECT_EQ(0, memcmp(&*received_packets_[i].data().data(),
                          &*data_.begin() + expected[i].start, expected_size));
      EXPECT_EQ(expected[i].flags, received_packets_[i].flags());
      EXPECT_EQ(expected[i].timestamp, received_packets_[i].timestamp());
    }
  }

  MessageLoop message_loop_;

  scoped_ptr<FakeSession> session_;
  scoped_ptr<RtpVideoReader> reader_;

  vector<char> data_;
  vector<VideoPacket> received_packets_;
};

// One non-fragmented packet marked as first.
TEST_F(RtpVideoReaderTest, NotFragmented_FirstPacket) {
  FragmentInfo fragments[] = {
    { 300, 123, true, false, Vp8Descriptor::NOT_FRAGMENTED, 0, 100 },
  };
  SplitAndSend(fragments, arraysize(fragments));

  ExpectedPacket expected[] = {
    { 123, VideoPacket::FIRST_PACKET, 0, 100 },
  };
  CheckResults(expected, arraysize(expected));
}

// One non-fragmented packet marked as last.
TEST_F(RtpVideoReaderTest, NotFragmented_LastPacket) {
  FragmentInfo fragments[] = {
    { 3000, 123, false, true, Vp8Descriptor::NOT_FRAGMENTED, 0, 100 },
  };
  SplitAndSend(fragments, arraysize(fragments));

  ExpectedPacket expected[] = {
    { 123, VideoPacket::LAST_PACKET, 0, 100 },
  };
  CheckResults(expected, arraysize(expected));
}

// Duplicated non-fragmented packet. Must be processed only once.
TEST_F(RtpVideoReaderTest, NotFragmented_Duplicate) {
  FragmentInfo fragments[] = {
    { 300, 123, true, false, Vp8Descriptor::NOT_FRAGMENTED, 0, 100 },
    { 300, 123, true, false, Vp8Descriptor::NOT_FRAGMENTED, 0, 100 },
  };
  SplitAndSend(fragments, arraysize(fragments));

  ExpectedPacket expected[] = {
    { 123, VideoPacket::FIRST_PACKET, 0, 100 },
  };
  CheckResults(expected, arraysize(expected));
}

// First packet split into two fragments.
TEST_F(RtpVideoReaderTest, TwoFragments_FirstPacket) {
  FragmentInfo fragments[] = {
    { 300, 321, true, false, Vp8Descriptor::FIRST_FRAGMENT, 0, 50 },
    { 301, 321, false, false, Vp8Descriptor::LAST_FRAGMENT, 50, 100 },
  };
  SplitAndSend(fragments, arraysize(fragments));

  ExpectedPacket expected[] = {
    { 321, VideoPacket::FIRST_PACKET, 0, 100 },
  };
  CheckResults(expected, arraysize(expected));
}

// Last packet split into two fragments.
TEST_F(RtpVideoReaderTest, TwoFragments_LastPacket) {
  FragmentInfo fragments[] = {
    { 3000, 400, false, false, Vp8Descriptor::FIRST_FRAGMENT, 0, 50 },
    { 3001, 400, false, true, Vp8Descriptor::LAST_FRAGMENT, 50, 100 },
  };
  SplitAndSend(fragments, arraysize(fragments));

  ExpectedPacket expected[] = {
    { 400, VideoPacket::LAST_PACKET, 0, 100 },
  };
  CheckResults(expected, arraysize(expected));
}

// Duplicated second fragment.
TEST_F(RtpVideoReaderTest, TwoFragments_WithDuplicate) {
  FragmentInfo fragments[] = {
    { 3000, 400, false, false, Vp8Descriptor::FIRST_FRAGMENT, 0, 50 },
    { 3001, 400, false, true, Vp8Descriptor::LAST_FRAGMENT, 50, 100 },
    { 3001, 400, false, true, Vp8Descriptor::LAST_FRAGMENT, 50, 100 },
  };
  SplitAndSend(fragments, arraysize(fragments));

  ExpectedPacket expected[] = {
    { 400, VideoPacket::LAST_PACKET, 0, 100 },
  };
  CheckResults(expected, arraysize(expected));
}

// Packet split into three fragments.
TEST_F(RtpVideoReaderTest, ThreeFragments_Ordered) {
  FragmentInfo fragments[] = {
    { 300, 400, true, false, Vp8Descriptor::FIRST_FRAGMENT, 0, 50 },
    { 301, 400, false, false, Vp8Descriptor::MIDDLE_FRAGMENT, 50, 90 },
    { 302, 400, false, false, Vp8Descriptor::LAST_FRAGMENT, 90, 100 },
  };
  SplitAndSend(fragments, arraysize(fragments));

  ExpectedPacket expected[] = {
    { 400, VideoPacket::FIRST_PACKET, 0, 100 },
  };
  CheckResults(expected, arraysize(expected));
}

// Packet split into three fragments received in reverse order.
TEST_F(RtpVideoReaderTest, ThreeFragments_ReverseOrder) {
  FragmentInfo fragments[] = {
    { 302, 400, false, false, Vp8Descriptor::LAST_FRAGMENT, 90, 100 },
    { 301, 400, false, false, Vp8Descriptor::MIDDLE_FRAGMENT, 50, 90 },
    { 300, 400, true, false, Vp8Descriptor::FIRST_FRAGMENT, 0, 50 },
  };
  SplitAndSend(fragments, arraysize(fragments));

  ExpectedPacket expected[] = {
    { 400, VideoPacket::FIRST_PACKET, 0, 100 },
  };
  CheckResults(expected, arraysize(expected));
}

// Two fragmented packets.
TEST_F(RtpVideoReaderTest, TwoPackets) {
  FragmentInfo fragments[] = {
    { 300, 100, true, false, Vp8Descriptor::FIRST_FRAGMENT, 0, 10 },
    { 301, 100, false, false, Vp8Descriptor::MIDDLE_FRAGMENT, 10, 20 },
    { 302, 100, false, false, Vp8Descriptor::LAST_FRAGMENT, 20, 40 },

    { 303, 200, false, false, Vp8Descriptor::FIRST_FRAGMENT, 40, 70 },
    { 304, 200, false, true, Vp8Descriptor::LAST_FRAGMENT, 70, 100 },
  };
  SplitAndSend(fragments, arraysize(fragments));

  ExpectedPacket expected[] = {
    { 100, VideoPacket::FIRST_PACKET, 0, 40 },
    { 200, VideoPacket::LAST_PACKET, 40, 100 },
  };
  CheckResults(expected, arraysize(expected));
}

// Sequence of three packets, with one lost fragment lost in the second packet.
TEST_F(RtpVideoReaderTest, LostFragment) {
  FragmentInfo fragments[] = {
    { 300, 100, true, false, Vp8Descriptor::FIRST_FRAGMENT, 0, 10 },
    { 301, 100, false, false, Vp8Descriptor::MIDDLE_FRAGMENT, 10, 20 },
    { 302, 100, false, false, Vp8Descriptor::LAST_FRAGMENT, 20, 30 },

    // Lost: { 303, 200, false, false, Vp8Descriptor::FIRST_FRAGMENT, 40, 50 },
    { 304, 200, false, true, Vp8Descriptor::LAST_FRAGMENT, 30, 40 },

    { 305, 300, true, false, Vp8Descriptor::FIRST_FRAGMENT, 50, 70 },
    { 306, 300, false, true, Vp8Descriptor::LAST_FRAGMENT, 70, 100 },
  };
  SplitAndSend(fragments, arraysize(fragments));

  ExpectedPacket expected[] = {
    { 100, VideoPacket::FIRST_PACKET, 0, 30 },
    { 300, VideoPacket::FIRST_PACKET | VideoPacket::LAST_PACKET, 50, 100 },
  };
  CheckResults(expected, arraysize(expected));
}

// Sequence of four packets, with two lost fragments.
TEST_F(RtpVideoReaderTest, TwoLostFragments) {
  // Fragments 303 and 306 should not be combined because they belong to
  // different Vp8 partitions.
  FragmentInfo fragments[] = {
    { 300, 100, true, false, Vp8Descriptor::FIRST_FRAGMENT, 0, 10 },
    { 301, 100, false, false, Vp8Descriptor::MIDDLE_FRAGMENT, 10, 20 },
    { 302, 100, false, false, Vp8Descriptor::LAST_FRAGMENT, 20, 30 },

    { 303, 200, false, false, Vp8Descriptor::FIRST_FRAGMENT, 40, 50 },
    // Lost: { 304, 200, false, true, Vp8Descriptor::LAST_FRAGMENT, 30, 40 },

    // Lost: { 305, 300, true, false, Vp8Descriptor::FIRST_FRAGMENT, 50, 60 },
    { 306, 300, false, true, Vp8Descriptor::LAST_FRAGMENT, 60, 70 },

    { 307, 400, true, false, Vp8Descriptor::FIRST_FRAGMENT, 70, 80 },
    { 308, 400, false, true, Vp8Descriptor::LAST_FRAGMENT, 80, 100 },
  };
  SplitAndSend(fragments, arraysize(fragments));

  ExpectedPacket expected[] = {
    { 100, VideoPacket::FIRST_PACKET, 0, 30 },
    { 400, VideoPacket::FIRST_PACKET | VideoPacket::LAST_PACKET, 70, 100 },
  };
  CheckResults(expected, arraysize(expected));
}

// Sequence number wrapping.
TEST_F(RtpVideoReaderTest, SequenceNumberWrapping) {
  FragmentInfo fragments[] = {
    { 65534, 400, true, false, Vp8Descriptor::FIRST_FRAGMENT, 0, 50 },
    { 65535, 400, false, false, Vp8Descriptor::MIDDLE_FRAGMENT, 50, 90 },
    { 0, 400, false, false, Vp8Descriptor::LAST_FRAGMENT, 90, 100 },
  };
  SplitAndSend(fragments, arraysize(fragments));

  ExpectedPacket expected[] = {
    { 400, VideoPacket::FIRST_PACKET, 0, 100 },
  };
  CheckResults(expected, arraysize(expected));
}

// Sequence number wrapping for fragments received out of order.
TEST_F(RtpVideoReaderTest, SequenceNumberWrappingReordered) {
  FragmentInfo fragments[] = {
    { 0, 400, false, false, Vp8Descriptor::LAST_FRAGMENT, 90, 100 },
    { 65534, 400, true, false, Vp8Descriptor::FIRST_FRAGMENT, 0, 50 },
    { 65535, 400, false, false, Vp8Descriptor::MIDDLE_FRAGMENT, 50, 90 },
  };
  SplitAndSend(fragments, arraysize(fragments));

  ExpectedPacket expected[] = {
    { 400, VideoPacket::FIRST_PACKET, 0, 100 },
  };
  CheckResults(expected, arraysize(expected));
}

// An old packet with invalid sequence number.
TEST_F(RtpVideoReaderTest, OldPacket) {
  FragmentInfo fragments[] = {
    { 32000, 123, true, true, Vp8Descriptor::NOT_FRAGMENTED, 0, 30 },

    // Should be ignored.
    { 10000, 532, true, true, Vp8Descriptor::NOT_FRAGMENTED, 30, 40 },

    { 32001, 223, true, true, Vp8Descriptor::NOT_FRAGMENTED, 40, 50 },
  };
  SplitAndSend(fragments, arraysize(fragments));

  ExpectedPacket expected[] = {
    { 123, VideoPacket::FIRST_PACKET | VideoPacket::LAST_PACKET, 0, 30 },
    { 223, VideoPacket::FIRST_PACKET | VideoPacket::LAST_PACKET, 40, 50 },
  };
  CheckResults(expected, arraysize(expected));
}

}  // namespace protocol
}  // namespace remoting
