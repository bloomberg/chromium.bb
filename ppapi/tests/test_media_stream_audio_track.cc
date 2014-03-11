// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Tests PPB_MediaStreamAudioTrack interface.

#include "ppapi/tests/test_media_stream_audio_track.h"

#include "ppapi/c/private/ppb_testing_private.h"
#include "ppapi/cpp/audio_buffer.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/var.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(MediaStreamAudioTrack);

namespace {

const int32_t kTimes = 3;
const char kJSCode[] =
    "function gotStream(stream) {"
    "  test_stream = stream;"
    "  var track = stream.getAudioTracks()[0];"
    "  var plugin = document.getElementById('plugin');"
    "  plugin.postMessage(track);"
    "}"
    "var constraints = {"
    "  audio: true,"
    "  video: false,"
    "};"
    "navigator.getUserMedia = "
    "    navigator.getUserMedia || navigator.webkitGetUserMedia;"
    "navigator.getUserMedia(constraints,"
    "    gotStream, function() {});";

// Helper to check if the |sample_rate| is listed in PP_AudioBuffer_SampleRate
// enum.
bool IsSampleRateValid(PP_AudioBuffer_SampleRate sample_rate) {
  switch (sample_rate) {
    case PP_AUDIOBUFFER_SAMPLERATE_8000:
    case PP_AUDIOBUFFER_SAMPLERATE_16000:
    case PP_AUDIOBUFFER_SAMPLERATE_22050:
    case PP_AUDIOBUFFER_SAMPLERATE_32000:
    case PP_AUDIOBUFFER_SAMPLERATE_44100:
    case PP_AUDIOBUFFER_SAMPLERATE_48000:
    case PP_AUDIOBUFFER_SAMPLERATE_96000:
    case PP_AUDIOBUFFER_SAMPLERATE_192000:
      return true;
    default:
      return false;
  }
}

}

TestMediaStreamAudioTrack::TestMediaStreamAudioTrack(TestingInstance* instance)
    : TestCase(instance),
      event_(instance_->pp_instance()) {
}

bool TestMediaStreamAudioTrack::Init() {
  return true;
}

TestMediaStreamAudioTrack::~TestMediaStreamAudioTrack() {
}

void TestMediaStreamAudioTrack::RunTests(const std::string& filter) {
  RUN_TEST(Create, filter);
  RUN_TEST(GetBuffer, filter);
}

void TestMediaStreamAudioTrack::HandleMessage(const pp::Var& message) {
  if (message.is_resource()) {
    audio_track_ = pp::MediaStreamAudioTrack(message.AsResource());
  }
  event_.Signal();
}

std::string TestMediaStreamAudioTrack::TestCreate() {
  // Create a track.
  instance_->EvalScript(kJSCode);
  event_.Wait();
  event_.Reset();

  ASSERT_FALSE(audio_track_.is_null());
  ASSERT_FALSE(audio_track_.HasEnded());
  ASSERT_FALSE(audio_track_.GetId().empty());

  // Close the track.
  audio_track_.Close();
  ASSERT_TRUE(audio_track_.HasEnded());
  audio_track_ = pp::MediaStreamAudioTrack();
  PASS();
}

std::string TestMediaStreamAudioTrack::TestGetBuffer() {
  // Create a track.
  instance_->EvalScript(kJSCode);
  event_.Wait();
  event_.Reset();

  ASSERT_FALSE(audio_track_.is_null());
  ASSERT_FALSE(audio_track_.HasEnded());
  ASSERT_FALSE(audio_track_.GetId().empty());

  PP_TimeDelta timestamp = 0.0;

  // Get |kTimes| buffers.
  for (int i = 0; i < kTimes; ++i) {
    TestCompletionCallbackWithOutput<pp::AudioBuffer> cc(
        instance_->pp_instance(), false);
    cc.WaitForResult(audio_track_.GetBuffer(cc.GetCallback()));
    ASSERT_EQ(PP_OK, cc.result());
    pp::AudioBuffer buffer = cc.output();
    ASSERT_FALSE(buffer.is_null());
    ASSERT_TRUE(IsSampleRateValid(buffer.GetSampleRate()));
    ASSERT_EQ(buffer.GetSampleSize(), PP_AUDIOBUFFER_SAMPLESIZE_16_BITS);

    ASSERT_GE(buffer.GetTimestamp(), timestamp);
    timestamp = buffer.GetTimestamp();

    ASSERT_GT(buffer.GetDataBufferSize(), 0U);
    ASSERT_TRUE(buffer.GetDataBuffer() != NULL);

    audio_track_.RecycleBuffer(buffer);

    // A recycled buffer should be invalidated.
    ASSERT_EQ(buffer.GetSampleRate(), PP_AUDIOBUFFER_SAMPLERATE_UNKNOWN);
    ASSERT_EQ(buffer.GetSampleSize(), PP_AUDIOBUFFER_SAMPLESIZE_UNKNOWN);
    ASSERT_EQ(buffer.GetDataBufferSize(), 0U);
    ASSERT_TRUE(buffer.GetDataBuffer() == NULL);
  }

  // Close the track.
  audio_track_.Close();
  ASSERT_TRUE(audio_track_.HasEnded());
  audio_track_ = pp::MediaStreamAudioTrack();
  PASS();
}
