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

// Real max defined in
// content/renderer/pepper/pepper_media_stream_audio_track_host.cc.
const int32_t kMaxNumberOfBuffers = 1000;
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

}  // namespace

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
  RUN_TEST(Configure, filter);
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

std::string TestMediaStreamAudioTrack::TestConfigure() {
  // Create a track.
  instance_->EvalScript(kJSCode);
  event_.Wait();
  event_.Reset();

  ASSERT_FALSE(audio_track_.is_null());
  ASSERT_FALSE(audio_track_.HasEnded());
  ASSERT_FALSE(audio_track_.GetId().empty());

  PP_TimeDelta timestamp = 0.0;

  // Configure number of buffers.
  struct {
    int32_t buffers;
    int32_t expect_result;
  } buffers[] = {
    { 8, PP_OK },
    { 100, PP_OK },
    { kMaxNumberOfBuffers, PP_OK },
    { -1, PP_ERROR_BADARGUMENT },
    { kMaxNumberOfBuffers + 1, PP_OK },  // Clipped to max value.
    { 0, PP_OK },  // Use default.
  };
  for (size_t i = 0; i < sizeof(buffers) / sizeof(buffers[0]); ++i) {
    TestCompletionCallback cc_configure(instance_->pp_instance(), false);
    int32_t attrib_list[] = {
      PP_MEDIASTREAMAUDIOTRACK_ATTRIB_BUFFERS, buffers[i].buffers,
      PP_MEDIASTREAMAUDIOTRACK_ATTRIB_NONE,
    };
    cc_configure.WaitForResult(
        audio_track_.Configure(attrib_list, cc_configure.GetCallback()));
    ASSERT_EQ(buffers[i].expect_result, cc_configure.result());

    // Get some buffers. This should also succeed when configure fails.
    for (int j = 0; j < kTimes; ++j) {
      TestCompletionCallbackWithOutput<pp::AudioBuffer> cc_get_buffer(
          instance_->pp_instance(), false);
      cc_get_buffer.WaitForResult(
          audio_track_.GetBuffer(cc_get_buffer.GetCallback()));
      ASSERT_EQ(PP_OK, cc_get_buffer.result());
      pp::AudioBuffer buffer = cc_get_buffer.output();
      ASSERT_FALSE(buffer.is_null());
      ASSERT_TRUE(IsSampleRateValid(buffer.GetSampleRate()));
      ASSERT_EQ(buffer.GetSampleSize(), PP_AUDIOBUFFER_SAMPLESIZE_16_BITS);

      ASSERT_GE(buffer.GetTimestamp(), timestamp);
      timestamp = buffer.GetTimestamp();

      ASSERT_GT(buffer.GetDataBufferSize(), 0U);
      ASSERT_TRUE(buffer.GetDataBuffer() != NULL);

      audio_track_.RecycleBuffer(buffer);
    }
  }

  // Configure should fail while plugin holds buffers.
  {
    TestCompletionCallbackWithOutput<pp::AudioBuffer> cc_get_buffer(
        instance_->pp_instance(), false);
    cc_get_buffer.WaitForResult(
        audio_track_.GetBuffer(cc_get_buffer.GetCallback()));
    ASSERT_EQ(PP_OK, cc_get_buffer.result());
    pp::AudioBuffer buffer = cc_get_buffer.output();
    int32_t attrib_list[] = {
      PP_MEDIASTREAMAUDIOTRACK_ATTRIB_BUFFERS, 0,
      PP_MEDIASTREAMAUDIOTRACK_ATTRIB_NONE,
    };
    TestCompletionCallback cc_configure(instance_->pp_instance(), false);
    cc_configure.WaitForResult(
        audio_track_.Configure(attrib_list, cc_configure.GetCallback()));
    ASSERT_EQ(PP_ERROR_INPROGRESS, cc_configure.result());
    audio_track_.RecycleBuffer(buffer);
  }

  // Close the track.
  audio_track_.Close();
  ASSERT_TRUE(audio_track_.HasEnded());
  audio_track_ = pp::MediaStreamAudioTrack();
  PASS();
}
