// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/audio_pump.h"

#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "remoting/codec/audio_encoder.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/proto/audio.pb.h"
#include "remoting/protocol/audio_stub.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

// Creates a dummy packet with 1k data
scoped_ptr<AudioPacket> MakeAudioPacket() {
  scoped_ptr<AudioPacket> packet(new AudioPacket);
  packet->add_data()->resize(1000);
  return packet.Pass();
}

}  // namespace

class FakeAudioCapturer : public AudioCapturer {
 public:
  FakeAudioCapturer() {}
  ~FakeAudioCapturer() override {}

  bool Start(const PacketCapturedCallback& callback) override {
    callback_ = callback;
    return true;
  }

  const PacketCapturedCallback& callback() { return callback_; }

 private:
  PacketCapturedCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeAudioCapturer);
};

class FakeAudioEncoder : public AudioEncoder {
 public:
  FakeAudioEncoder() {}
  ~FakeAudioEncoder() override {}

  scoped_ptr<AudioPacket> Encode(scoped_ptr<AudioPacket> packet) override {
    return packet.Pass();
  }
  int GetBitrate() override {
    return 160000;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeAudioEncoder);
};

class AudioPumpTest : public testing::Test, public protocol::AudioStub {
 public:
  AudioPumpTest() {}

  void SetUp() override;
  void TearDown() override;

  // protocol::AudioStub interface.
  void ProcessAudioPacket(scoped_ptr<AudioPacket> audio_packet,
                          const base::Closure& done) override;

 protected:
  base::MessageLoop message_loop_;

  // |capturer_| and |encoder_| are owned by the |pump_|.
  FakeAudioCapturer* capturer_;
  FakeAudioEncoder* encoder_;

  scoped_ptr<AudioPump> pump_;

  ScopedVector<AudioPacket> sent_packets_;
  std::vector<base::Closure> done_closures_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioPumpTest);
};

void AudioPumpTest::SetUp() {
  capturer_ = new FakeAudioCapturer();
  encoder_ = new FakeAudioEncoder();
  pump_.reset(new AudioPump(message_loop_.task_runner(),
                            make_scoped_ptr(capturer_),
                            make_scoped_ptr(encoder_), this));
}

void AudioPumpTest::TearDown() {
  pump_.reset();

  // Let the message loop run to finish destroying the capturer.
  base::RunLoop().RunUntilIdle();
}

void AudioPumpTest::ProcessAudioPacket(
    scoped_ptr<AudioPacket> audio_packet,
    const base::Closure& done) {
  sent_packets_.push_back(audio_packet.Pass());
  done_closures_.push_back(done);
}

// Verify that the pump pauses pumping when the network is congested.
TEST_F(AudioPumpTest, BufferSizeLimit) {
  // Run message loop to let the pump start the capturer.
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(capturer_->callback().is_null());

  // Try sending 100 packets, 1k each. The pump should stop pumping and start
  // dropping the data at some point.
  for (size_t i = 0; i < 100; ++i) {
    capturer_->callback().Run(MakeAudioPacket());
    base::RunLoop().RunUntilIdle();
  }

  size_t num_sent_packets = sent_packets_.size();
  EXPECT_LT(num_sent_packets, 100U);
  EXPECT_GT(num_sent_packets, 0U);

  // Call done closure for the first packet. This should allow one more packet
  // to be sent below.
  done_closures_.front().Run();
  base::RunLoop().RunUntilIdle();

  // Verify that the pump continues to send captured audio.
  capturer_->callback().Run(MakeAudioPacket());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(num_sent_packets + 1, sent_packets_.size());
}

}  // namespace remoting
