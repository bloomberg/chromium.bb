// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "remoting/base/decoder.h"
#include "remoting/client/chromoting_view.h"
#include "remoting/proto/internal.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;

namespace remoting {

class MockDecoder : public Decoder {
 public:
  MockDecoder() {}

  MOCK_METHOD4(BeginDecode, bool(scoped_refptr<media::VideoFrame> frame,
                                 UpdatedRects* updated_rects,
                                 Task* partial_decode_done,
                                 Task* decode_done));
  MOCK_METHOD1(PartialDecode, bool(ChromotingHostMessage* message));
  MOCK_METHOD0(EndDecode, void());

  MOCK_METHOD0(Encoding, UpdateStreamEncoding());
  MOCK_METHOD0(IsStarted, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDecoder);
};

// Fake ChromotingView that provides stub implementations for all pure virtual
// methods. This is sufficient since we're only interested in testing the
// base class methods in this file.
class FakeView : public ChromotingView {
  bool Initialize() { return false; }
  void TearDown() {}
  void Paint() {}
  void SetSolidFill(uint32 color) {}
  void UnsetSolidFill() {}
  void SetConnectionState(ConnectionState s) {}
  void SetViewport(int x, int y, int width, int height) {
    frame_width_ = width;
    frame_height_ = height;
  }
  void SetHostScreenSize(int width, int height) {}
  void HandleBeginUpdateStream(ChromotingHostMessage* msg) {}
  void HandleUpdateStreamPacket(ChromotingHostMessage* msg) {}
  void HandleEndUpdateStream(ChromotingHostMessage* msg) {}

 public:
  // Testing accessors.
  // These provide access to private/protected members of ChromotingView so
  // that they can be tested/verified.
  Decoder* get_decoder() {
    return decoder_.get();
  }
  void set_decoder(Decoder* decoder) {
    decoder_.reset(decoder);
  }

  // Testing wrappers for private setup/startup decoder routines.
  bool setup_decoder(UpdateStreamEncoding encoding) {
    return SetupDecoder(encoding);
  }
  bool begin_decoding(Task* partial_decode_done, Task* decode_done) {
    return BeginDecoding(partial_decode_done, decode_done);
  }
  bool decode(ChromotingHostMessage* msg) {
    return Decode(msg);
  }
  bool end_decoding() {
    return EndDecoding();
  }

  // Testing setup.
  void set_test_viewport() {
    SetViewport(0, 0, 640, 480);
  }
};

// Verify the initial state.
TEST(ChromotingViewTest, InitialState) {
  scoped_ptr<FakeView> view(new FakeView());
  EXPECT_TRUE(view->get_decoder() == NULL);
}

// Test a simple decoder sequence:
//   HandleBeginUpdateStream:
//   HandleUpdateStreamPacket:
//     SetupDecoder - return false
//     BeginDecoding
//     Decode
//   HandleEndUpdateStream:
//     EndDecoding
TEST(ChromotingViewTest, DecodeSimple) {
  scoped_ptr<FakeView> view(new FakeView());
  view->set_test_viewport();

  // HandleBeginUpdateStream

  // HandleUpdateStreamPacket

  ASSERT_TRUE(view->setup_decoder(EncodingZlib));
  Decoder* decoder = view->get_decoder();
  ASSERT_TRUE(decoder != NULL);
  EXPECT_EQ(EncodingZlib, decoder->Encoding());
  EXPECT_FALSE(decoder->IsStarted());

  // Overwrite |decoder_| with MockDecoder.
  MockDecoder* mock_decoder = new MockDecoder();
  view->set_decoder(mock_decoder);
  EXPECT_CALL(*mock_decoder, Encoding())
      .WillRepeatedly(Return(EncodingZlib));
  {
    InSequence s;

    // BeginDecoding
    EXPECT_CALL(*mock_decoder, IsStarted())
        .WillOnce(Return(false));
    EXPECT_CALL(*mock_decoder, BeginDecode(_, _, _, _))
        .WillOnce(Return(true));
    EXPECT_CALL(*mock_decoder, IsStarted())
        .WillOnce(Return(true));

    // Decode
    EXPECT_CALL(*mock_decoder, IsStarted())
        .WillOnce(Return(true));
    EXPECT_CALL(*mock_decoder, PartialDecode(_))
        .WillOnce(Return(true));

    // EndDecoding
    EXPECT_CALL(*mock_decoder, IsStarted())
        .WillOnce(Return(true));
    EXPECT_CALL(*mock_decoder, EndDecode())
        .Times(1);
    EXPECT_CALL(*mock_decoder, IsStarted())
        .WillOnce(Return(false));
  }

  // decoder_->IsStarted() is false, so we call begin_decoding().
  ASSERT_TRUE(view->begin_decoding(NULL, NULL));

  ASSERT_TRUE(view->decode(NULL));

  // HandleEndUpdateStream

  ASSERT_TRUE(view->end_decoding());
}

// Test a three-packet decoder sequence:
//   HandleBeginUpdateStream:
//   HandleUpdateStreamPacket: (1)
//     SetupDecoder - return false
//     BeginDecoding
//     Decode
//   HandleUpdateStreamPacket: (2)
//     SetupDecoder - return true
//     Decode
//   HandleUpdateStreamPacket: (3)
//     SetupDecoder - return true
//     Decode
//   HandleEndUpdateStream:
//     EndDecoding
TEST(ChromotingViewTest, DecodeThreePackets) {
  scoped_ptr<FakeView> view(new FakeView());
  view->set_test_viewport();

  // HandleBeginUpdateStream

  // HandleUpdateStreamPacket (1)

  ASSERT_TRUE(view->setup_decoder(EncodingZlib));
  Decoder* decoder = view->get_decoder();
  ASSERT_TRUE(decoder != NULL);
  EXPECT_EQ(EncodingZlib, decoder->Encoding());
  EXPECT_FALSE(decoder->IsStarted());

  // Overwrite |decoder_| with MockDecoder.
  MockDecoder* mock_decoder = new MockDecoder();
  view->set_decoder(mock_decoder);
  EXPECT_CALL(*mock_decoder, Encoding())
      .WillRepeatedly(Return(EncodingZlib));
  EXPECT_CALL(*mock_decoder, BeginDecode(_, _, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_decoder, PartialDecode(_))
      .Times(3)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_decoder, EndDecode())
      .Times(1);
  {
    InSequence s;

    // BeginDecoding
    EXPECT_CALL(*mock_decoder, IsStarted())
        .WillOnce(Return(false))
        .RetiresOnSaturation();
    // BeginDecoding (1)
    // Decode (1)
    // SetupDecoder (1)
    // Decode (1)
    // SetupDecoder (1)
    // Decode (1)
    // EndDecoding (1)
    // Total = 7 calls
    EXPECT_CALL(*mock_decoder, IsStarted())
        .Times(7)
        .WillRepeatedly(Return(true))
        .RetiresOnSaturation();
    // EndDecoding
    EXPECT_CALL(*mock_decoder, IsStarted())
        .WillOnce(Return(false))
        .RetiresOnSaturation();
  }

  // decoder_->IsStarted() is false, so we call begin_decoding().
  ASSERT_TRUE(view->begin_decoding(NULL, NULL));

  ASSERT_TRUE(view->decode(NULL));

  // HandleUpdateStreamPacket (2)

  ASSERT_TRUE(view->setup_decoder(EncodingZlib));
  // Don't call BeginDecoding() because it is already started.
  ASSERT_TRUE(view->decode(NULL));

  // HandleUpdateStreamPacket (3)

  ASSERT_TRUE(view->setup_decoder(EncodingZlib));
  // Don't call BeginDecoding() because it is already started.
  ASSERT_TRUE(view->decode(NULL));

  // HandleEndUpdateStream

  ASSERT_TRUE(view->end_decoding());
}

// Test two update streams: (same packet encoding)
//   HandleBeginUpdateStream:
//   HandleUpdateStreamPacket:
//     SetupDecoder - return false
//     BeginDecoding
//     Decode
//   HandleEndUpdateStream:
//     EndDecoding
//
//   HandleBeginUpdateStream:
//   HandleUpdateStreamPacket:
//     SetupDecoder - return false
//     BeginDecoding
//     Decode
//   HandleEndUpdateStream:
//     EndDecoding
TEST(ChromotingViewTest, DecodeTwoStreams) {
  scoped_ptr<FakeView> view(new FakeView());
  view->set_test_viewport();

  // HandleBeginUpdateStream (update stream 1)

  // HandleUpdateStreamPacket

  ASSERT_TRUE(view->setup_decoder(EncodingZlib));
  Decoder* decoder = view->get_decoder();
  ASSERT_TRUE(decoder != NULL);
  EXPECT_EQ(EncodingZlib, decoder->Encoding());
  EXPECT_FALSE(decoder->IsStarted());

  // Overwrite |decoder_| with MockDecoder.
  MockDecoder* mock_decoder = new MockDecoder();
  view->set_decoder(mock_decoder);
  EXPECT_CALL(*mock_decoder, Encoding())
      .WillRepeatedly(Return(EncodingZlib));
  EXPECT_CALL(*mock_decoder, BeginDecode(_, _, _, _))
      .Times(2)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_decoder, PartialDecode(_))
      .Times(2)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_decoder, EndDecode())
      .Times(2);
  {
    InSequence s;
    // BeginDecoding
    EXPECT_CALL(*mock_decoder, IsStarted())
        .WillOnce(Return(false))
        .RetiresOnSaturation();
    // BeginDecoding (1)
    // Decode (1)
    // EndDecoding (1)
    // Total = 3 calls
    EXPECT_CALL(*mock_decoder, IsStarted())
        .Times(3)
        .WillRepeatedly(Return(true))
        .RetiresOnSaturation();
    // EndDecoding (1)
    // SetupDecoder (1)
    // BeginDecoding (1)
    // Total = 3 calls
    EXPECT_CALL(*mock_decoder, IsStarted())
        .Times(3)
        .WillRepeatedly(Return(false))
        .RetiresOnSaturation();
    // BeginDecoding (1)
    // Decode (1)
    // EndDecoding (1)
    // Total = 3 calls
    EXPECT_CALL(*mock_decoder, IsStarted())
        .Times(3)
        .WillRepeatedly(Return(true))
        .RetiresOnSaturation();
    // EndDecoding
    EXPECT_CALL(*mock_decoder, IsStarted())
        .WillOnce(Return(false))
        .RetiresOnSaturation();
  }

  // |started| is false, so we call begin_decoding().
  ASSERT_TRUE(view->begin_decoding(NULL, NULL));

  ASSERT_TRUE(view->decode(NULL));

  // HandleEndUpdateStream

  ASSERT_TRUE(view->end_decoding());

  // HandleBeginUpdateStream (update stream 2)

  // HandleUpdateStreamPacket

  Decoder* old_decoder = view->get_decoder();
  view->setup_decoder(EncodingZlib);
  // Verify we're using the same decoder since the encoding matches.
  Decoder* new_decoder = view->get_decoder();
  ASSERT_TRUE(new_decoder == old_decoder);

  // |started| is false, so we call begin_decoding().
  ASSERT_TRUE(view->begin_decoding(NULL, NULL));

  ASSERT_TRUE(view->decode(NULL));

  // HandleEndUpdateStream

  ASSERT_TRUE(view->end_decoding());
}

// Test two update streams with different encodings:
//   HandleBeginUpdateStream:
//   HandleUpdateStreamPacket: ('Zlib' encoded)
//     SetupDecoder
//     BeginDecoding
//     Decode
//   HandleEndUpdateStream:
//     EndDecoding
//
//   HandleBeginUpdateStream:
//   HandleUpdateStreamPacket: ('None' encoded)
//     SetupDecoder
//     BeginDecoding
//     Decode
//   HandleEndUpdateStream:
//     EndDecoding
TEST(ChromotingViewTest, DecodeTwoStreamsDifferentEncodings) {
  scoped_ptr<FakeView> view(new FakeView());
  view->set_test_viewport();

  // HandleBeginUpdateStream (update stream 1)

  // HandleUpdateStreamPacket

  ASSERT_TRUE(view->setup_decoder(EncodingZlib));
  Decoder* decoder = view->get_decoder();
  ASSERT_TRUE(decoder != NULL);
  EXPECT_EQ(EncodingZlib, decoder->Encoding());
  EXPECT_FALSE(decoder->IsStarted());

  // Overwrite |decoder_| with MockDecoder.
  MockDecoder* mock_decoder1 = new MockDecoder();
  view->set_decoder(mock_decoder1);
  EXPECT_CALL(*mock_decoder1, Encoding())
      .WillRepeatedly(Return(EncodingZlib));
  EXPECT_CALL(*mock_decoder1, BeginDecode(_, _, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_decoder1, PartialDecode(_))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_decoder1, EndDecode());
  {
    InSequence s1;
    // BeginDecoding
    EXPECT_CALL(*mock_decoder1, IsStarted())
        .WillOnce(Return(false))
        .RetiresOnSaturation();
    // BeginDecoding (1)
    // Decode (1)
    // EndDecoding (1)
    // Total = 3 calls
    EXPECT_CALL(*mock_decoder1, IsStarted())
        .Times(3)
        .WillRepeatedly(Return(true))
        .RetiresOnSaturation();
    // EndDecoding (1)
    // SetupDecoder (1)
    // Total = 2 calls
    EXPECT_CALL(*mock_decoder1, IsStarted())
        .Times(2)
        .WillRepeatedly(Return(false))
        .RetiresOnSaturation();
  }

  // |started| is false, so we call begin_decoding().
  ASSERT_TRUE(view->begin_decoding(NULL, NULL));

  ASSERT_TRUE(view->decode(NULL));

  // HandleEndUpdateStream

  ASSERT_TRUE(view->end_decoding());

  // HandleBeginUpdateStream (update stream 2)

  // HandleUpdateStreamPacket

  // Encoding for second stream is different from first, so this will
  // create a new decoder.
  ASSERT_TRUE(view->setup_decoder(EncodingNone));
  // The decoder should be new.
  EXPECT_NE(mock_decoder1, view->get_decoder());

  // Overwrite |decoder_| with MockDecoder.
  MockDecoder* mock_decoder2 = new MockDecoder();
  view->set_decoder(mock_decoder2);
  EXPECT_CALL(*mock_decoder2, Encoding())
      .WillRepeatedly(Return(EncodingNone));
  EXPECT_CALL(*mock_decoder2, BeginDecode(_, _, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_decoder2, PartialDecode(_))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_decoder2, EndDecode());
  {
    InSequence s2;
    // BeginDecoding
    EXPECT_CALL(*mock_decoder2, IsStarted())
        .WillOnce(Return(false))
        .RetiresOnSaturation();
    // BeginDecoding (1)
    // Decode (1)
    // EndDecoding (1)
    // Total = 3 calls
    EXPECT_CALL(*mock_decoder2, IsStarted())
        .Times(3)
        .WillRepeatedly(Return(true))
        .RetiresOnSaturation();
    // EndDecoding
    EXPECT_CALL(*mock_decoder2, IsStarted())
        .WillOnce(Return(false))
        .RetiresOnSaturation();
  }

  // |started| is false, so we call begin_decoding().
  ASSERT_TRUE(view->begin_decoding(NULL, NULL));

  ASSERT_TRUE(view->decode(NULL));

  // HandleEndUpdateStream

  ASSERT_TRUE(view->end_decoding());
}

// Test failure when packets in a stream have mismatched encodings.
//   HandleBeginUpdateStream:
//   HandleUpdateStreamPacket: (1)
//     SetupDecoder - encoding = Zlib
//     BeginDecoding
//     Decode
//   HandleUpdateStreamPacket: (2)
//     SetupDecoder - encoding = none
//     DEATH
TEST(ChromotingViewTest, MismatchedEncodings) {
  scoped_ptr<FakeView> view(new FakeView());
  view->set_test_viewport();

  // HandleBeginUpdateStream

  // HandleUpdateStreamPacket (1)
  // encoding = Zlib

  ASSERT_TRUE(view->setup_decoder(EncodingZlib));
  Decoder* decoder = view->get_decoder();
  ASSERT_TRUE(decoder != NULL);
  EXPECT_EQ(EncodingZlib, decoder->Encoding());
  EXPECT_FALSE(decoder->IsStarted());

  // Overwrite |decoder_| with MockDecoder.
  MockDecoder* mock_decoder = new MockDecoder();
  view->set_decoder(mock_decoder);
  EXPECT_CALL(*mock_decoder, Encoding())
      .WillRepeatedly(Return(EncodingZlib));
  {
    InSequence s;

    // BeginDecoding().
    EXPECT_CALL(*mock_decoder, IsStarted())
        .WillOnce(Return(false));
    EXPECT_CALL(*mock_decoder, BeginDecode(_, _, _, _))
        .WillOnce(Return(true));
    EXPECT_CALL(*mock_decoder, IsStarted())
        .WillOnce(Return(true));

    // Decode().
    EXPECT_CALL(*mock_decoder, IsStarted())
        .WillOnce(Return(true));
    EXPECT_CALL(*mock_decoder, PartialDecode(_))
        .WillOnce(Return(true));

    // SetupDecoder().
    EXPECT_CALL(*mock_decoder, IsStarted())
        .WillOnce(Return(true));
  }

  // |started| is false, so we call begin_decoding().
  ASSERT_TRUE(view->begin_decoding(NULL, NULL));

  ASSERT_TRUE(view->decode(NULL));

  // HandleUpdateStreamPacket (2)
  // encoding = None

  // Doesn't match previous packet encoding, so returns failure.
  ASSERT_FALSE(view->setup_decoder(EncodingNone));
}

// Verify we fail when Decode() is called without first calling
// BeginDecoding().
TEST(ChromotingViewTest, MissingBegin) {
  scoped_ptr<FakeView> view(new FakeView());
  view->set_test_viewport();

  ASSERT_TRUE(view->setup_decoder(EncodingZlib));
  ASSERT_TRUE(view->get_decoder() != NULL);

  // Overwrite |decoder_| with MockDecoder.
  MockDecoder* mock_decoder = new MockDecoder();
  view->set_decoder(mock_decoder);
  EXPECT_CALL(*mock_decoder, IsStarted())
      .WillOnce(Return(false));

  // Call decode() without calling begin_decoding().
  EXPECT_FALSE(view->decode(NULL));
}

// Test requesting a decoder for an invalid encoding.
TEST(ChromotingViewTest, InvalidEncoding) {
  scoped_ptr<FakeView> view(new FakeView());
  EXPECT_FALSE(view->setup_decoder(EncodingInvalid));
}

}  // namespace remoting
