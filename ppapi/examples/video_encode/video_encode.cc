// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <iostream>
#include <sstream>
#include <vector>

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_console.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/media_stream_video_track.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/var_array_buffer.h"
#include "ppapi/cpp/var_dictionary.h"
#include "ppapi/cpp/video_encoder.h"
#include "ppapi/cpp/video_frame.h"
#include "ppapi/utility/completion_callback_factory.h"

// TODO(llandwerlin): turn on by default when we have software encode.
// #define USE_VP8_INSTEAD_OF_H264

// When compiling natively on Windows, PostMessage can be #define-d to
// something else.
#ifdef PostMessage
#undef PostMessage
#endif

// Use assert as a poor-man's CHECK, even in non-debug mode.
// Since <assert.h> redefines assert on every inclusion (it doesn't use
// include-guards), make sure this is the last file #include'd in this file.
#undef NDEBUG
#include <assert.h>

namespace {

std::string VideoProfileToString(PP_VideoProfile profile) {
  switch (profile) {
    case PP_VIDEOPROFILE_H264BASELINE:
      return "h264baseline";
    case PP_VIDEOPROFILE_H264MAIN:
      return "h264main";
    case PP_VIDEOPROFILE_H264EXTENDED:
      return "h264extended";
    case PP_VIDEOPROFILE_H264HIGH:
      return "h264high";
    case PP_VIDEOPROFILE_H264HIGH10PROFILE:
      return "h264high10";
    case PP_VIDEOPROFILE_H264HIGH422PROFILE:
      return "h264high422";
    case PP_VIDEOPROFILE_H264HIGH444PREDICTIVEPROFILE:
      return "h264high444predictive";
    case PP_VIDEOPROFILE_H264SCALABLEBASELINE:
      return "h264scalablebaseline";
    case PP_VIDEOPROFILE_H264SCALABLEHIGH:
      return "h264scalablehigh";
    case PP_VIDEOPROFILE_H264STEREOHIGH:
      return "h264stereohigh";
    case PP_VIDEOPROFILE_H264MULTIVIEWHIGH:
      return "h264multiviewhigh";
    case PP_VIDEOPROFILE_VP8_ANY:
      return "vp8";
    case PP_VIDEOPROFILE_VP9_ANY:
      return "vp9";
    // No default to catch unhandled profiles.
  }
  return "unknown";
}

std::string HardwareAccelerationToString(PP_HardwareAcceleration acceleration) {
  switch (acceleration) {
    case PP_HARDWAREACCELERATION_ONLY:
      return "hardware";
    case PP_HARDWAREACCELERATION_WITHFALLBACK:
      return "hardware/software";
    case PP_HARDWAREACCELERATION_NONE:
      return "software";
    // No default to catch unhandled accelerations.
  }
  return "unknown";
}

// This object is the global object representing this plugin library as long
// as it is loaded.
class VideoEncoderModule : public pp::Module {
 public:
  VideoEncoderModule() : pp::Module() {}
  virtual ~VideoEncoderModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance);
};

class VideoEncoderInstance : public pp::Instance {
 public:
  VideoEncoderInstance(PP_Instance instance, pp::Module* module);
  virtual ~VideoEncoderInstance();

  // pp::Instance implementation.
  virtual void HandleMessage(const pp::Var& var_message);

 private:
  void ConfigureTrack();
  void OnConfiguredTrack(int32_t result);
  void ProbeEncoder();
  void OnEncoderProbed(int32_t result,
                       const std::vector<PP_VideoProfileDescription> profiles);
  void OnInitializedEncoder(int32_t result);
  void ScheduleNextEncode();
  void GetEncoderFrameTick(int32_t result);
  void GetEncoderFrame(const pp::VideoFrame& track_frame);
  void OnEncoderFrame(int32_t result,
                      pp::VideoFrame encoder_frame,
                      pp::VideoFrame track_frame);
  int32_t CopyVideoFrame(pp::VideoFrame dest, pp::VideoFrame src);
  void EncodeFrame(const pp::VideoFrame& frame);
  void OnEncodeDone(int32_t result);
  void OnGetBitstreamBuffer(int32_t result, PP_BitstreamBuffer buffer);
  void StartTrackFrames();
  void StopTrackFrames();
  void OnTrackFrame(int32_t result, pp::VideoFrame frame);

  void StopEncode();

  void LogError(int32_t error, const std::string& message);
  void Log(const std::string& message);

  void PostDataMessage(const void* buffer, uint32_t size);
  void PostSignalMessage(const char* name);

  bool is_encoding_;
  bool is_receiving_track_frames_;

  pp::VideoEncoder video_encoder_;
  pp::MediaStreamVideoTrack video_track_;
  pp::CompletionCallbackFactory<VideoEncoderInstance> callback_factory_;

  PP_VideoProfile video_profile_;
  PP_VideoFrame_Format frame_format_;

  pp::Size requested_size_;
  pp::Size frame_size_;
  pp::Size encoder_size_;
  uint32_t encoded_frames_;

  pp::VideoFrame current_track_frame_;
};

VideoEncoderInstance::VideoEncoderInstance(PP_Instance instance,
                                           pp::Module* module)
    : pp::Instance(instance),
      is_encoding_(false),
      callback_factory_(this),
#if defined(USE_VP8_INSTEAD_OF_H264)
      video_profile_(PP_VIDEOPROFILE_VP8_ANY),
#else
      video_profile_(PP_VIDEOPROFILE_H264MAIN),
#endif
      frame_format_(PP_VIDEOFRAME_FORMAT_I420),
      encoded_frames_(0) {
}

VideoEncoderInstance::~VideoEncoderInstance() {
}

void VideoEncoderInstance::ConfigureTrack() {
  if (encoder_size_.IsEmpty())
    frame_size_ = requested_size_;
  else
    frame_size_ = encoder_size_;

  int32_t attrib_list[] = {PP_MEDIASTREAMVIDEOTRACK_ATTRIB_FORMAT,
                           frame_format_,
                           PP_MEDIASTREAMVIDEOTRACK_ATTRIB_WIDTH,
                           frame_size_.width(),
                           PP_MEDIASTREAMVIDEOTRACK_ATTRIB_HEIGHT,
                           frame_size_.height(),
                           PP_MEDIASTREAMVIDEOTRACK_ATTRIB_NONE};

  pp::VarDictionary dict;
  dict.Set(pp::Var("status"), pp::Var("configuring video track"));
  dict.Set(pp::Var("width"), pp::Var(frame_size_.width()));
  dict.Set(pp::Var("height"), pp::Var(frame_size_.height()));
  PostMessage(dict);

  video_track_.Configure(
      attrib_list,
      callback_factory_.NewCallback(&VideoEncoderInstance::OnConfiguredTrack));
}

void VideoEncoderInstance::OnConfiguredTrack(int32_t result) {
  if (result != PP_OK) {
    LogError(result, "Cannot configure track");
    return;
  }

  if (is_encoding_) {
    StartTrackFrames();
    ScheduleNextEncode();
  } else
    ProbeEncoder();
}

void VideoEncoderInstance::ProbeEncoder() {
  video_encoder_ = pp::VideoEncoder(this);
  video_encoder_.GetSupportedProfiles(callback_factory_.NewCallbackWithOutput(
      &VideoEncoderInstance::OnEncoderProbed));
}

void VideoEncoderInstance::OnEncoderProbed(
    int32_t result,
    const std::vector<PP_VideoProfileDescription> profiles) {
  bool has_required_profile = false;

  Log("Available profiles:");
  for (const PP_VideoProfileDescription& profile : profiles) {
    std::ostringstream oss;
    oss << " profile=" << VideoProfileToString(profile.profile)
        << " max_resolution=" << profile.max_resolution.width << "x"
        << profile.max_resolution.height
        << " max_framerate=" << profile.max_framerate_numerator << "/"
        << profile.max_framerate_denominator << " acceleration="
        << HardwareAccelerationToString(profile.acceleration);
    Log(oss.str());

    has_required_profile |= profile.profile == video_profile_;
  }

  if (!has_required_profile) {
    std::ostringstream oss;
    oss << "Cannot find required video profile: ";
    oss << VideoProfileToString(video_profile_);
    LogError(PP_ERROR_FAILED, oss.str());
    return;
  }

  video_encoder_ = pp::VideoEncoder(this);

  pp::VarDictionary dict;
  dict.Set(pp::Var("status"), pp::Var("initializing encoder"));
  dict.Set(pp::Var("width"), pp::Var(encoder_size_.width()));
  dict.Set(pp::Var("height"), pp::Var(encoder_size_.height()));
  PostMessage(dict);

  int32_t error = video_encoder_.Initialize(
      frame_format_, frame_size_, video_profile_, 2000000,
      PP_HARDWAREACCELERATION_WITHFALLBACK,
      callback_factory_.NewCallback(
          &VideoEncoderInstance::OnInitializedEncoder));
  if (error != PP_OK_COMPLETIONPENDING) {
    LogError(error, "Cannot initialize encoder");
    return;
  }
}

void VideoEncoderInstance::OnInitializedEncoder(int32_t result) {
  if (result != PP_OK) {
    LogError(result, "Encoder initialization failed");
    return;
  }

  is_encoding_ = true;

  if (video_encoder_.GetFrameCodedSize(&encoder_size_) != PP_OK) {
    LogError(result, "Cannot get encoder coded frame size");
    return;
  }

  pp::VarDictionary dict;
  dict.Set(pp::Var("status"), pp::Var("encoder initialized"));
  dict.Set(pp::Var("width"), pp::Var(encoder_size_.width()));
  dict.Set(pp::Var("height"), pp::Var(encoder_size_.height()));
  PostMessage(dict);

  video_encoder_.GetBitstreamBuffer(callback_factory_.NewCallbackWithOutput(
      &VideoEncoderInstance::OnGetBitstreamBuffer));

  if (encoder_size_ != frame_size_)
    ConfigureTrack();
  else {
    StartTrackFrames();
    ScheduleNextEncode();
  }
}

void VideoEncoderInstance::ScheduleNextEncode() {
  pp::Module::Get()->core()->CallOnMainThread(
      1000 / 30,
      callback_factory_.NewCallback(&VideoEncoderInstance::GetEncoderFrameTick),
      0);
}

void VideoEncoderInstance::GetEncoderFrameTick(int32_t result) {
  if (is_encoding_) {
    if (!current_track_frame_.is_null()) {
      pp::VideoFrame frame = current_track_frame_;
      current_track_frame_.detach();
      GetEncoderFrame(frame);
    }
    ScheduleNextEncode();
  }
}

void VideoEncoderInstance::GetEncoderFrame(const pp::VideoFrame& track_frame) {
  video_encoder_.GetVideoFrame(callback_factory_.NewCallbackWithOutput(
      &VideoEncoderInstance::OnEncoderFrame, track_frame));
}

void VideoEncoderInstance::OnEncoderFrame(int32_t result,
                                          pp::VideoFrame encoder_frame,
                                          pp::VideoFrame track_frame) {
  if (result == PP_ERROR_ABORTED) {
    video_track_.RecycleFrame(track_frame);
    return;
  }
  if (result != PP_OK) {
    video_track_.RecycleFrame(track_frame);
    LogError(result, "Cannot get video frame from video encoder");
    return;
  }

  track_frame.GetSize(&frame_size_);

  if (frame_size_ != encoder_size_) {
    video_track_.RecycleFrame(track_frame);
    LogError(PP_ERROR_FAILED, "MediaStreamVideoTrack frame size incorrect");
    return;
  }

  if (CopyVideoFrame(encoder_frame, track_frame) == PP_OK)
    EncodeFrame(encoder_frame);
  video_track_.RecycleFrame(track_frame);
}

int32_t VideoEncoderInstance::CopyVideoFrame(pp::VideoFrame dest,
                                             pp::VideoFrame src) {
  if (dest.GetDataBufferSize() < src.GetDataBufferSize()) {
    std::ostringstream oss;
    oss << "Incorrect destination video frame buffer size : "
        << dest.GetDataBufferSize() << " < " << src.GetDataBufferSize();
    LogError(PP_ERROR_FAILED, oss.str());
    return PP_ERROR_FAILED;
  }

  memcpy(dest.GetDataBuffer(), src.GetDataBuffer(), src.GetDataBufferSize());
  return PP_OK;
}

void VideoEncoderInstance::EncodeFrame(const pp::VideoFrame& frame) {
  video_encoder_.Encode(
      frame, PP_FALSE,
      callback_factory_.NewCallback(&VideoEncoderInstance::OnEncodeDone));
}

void VideoEncoderInstance::OnEncodeDone(int32_t result) {
  if (result == PP_ERROR_ABORTED)
    return;
  if (result != PP_OK)
    LogError(result, "Encode failed");
}

void VideoEncoderInstance::OnGetBitstreamBuffer(int32_t result,
                                                PP_BitstreamBuffer buffer) {
  if (result == PP_ERROR_ABORTED)
    return;
  if (result != PP_OK) {
    LogError(result, "Cannot get bitstream buffer");
    return;
  }

  encoded_frames_++;
  PostDataMessage(buffer.buffer, buffer.size);
  video_encoder_.RecycleBitstreamBuffer(buffer);

  video_encoder_.GetBitstreamBuffer(callback_factory_.NewCallbackWithOutput(
      &VideoEncoderInstance::OnGetBitstreamBuffer));
}

void VideoEncoderInstance::StartTrackFrames() {
  is_receiving_track_frames_ = true;
  video_track_.GetFrame(callback_factory_.NewCallbackWithOutput(
      &VideoEncoderInstance::OnTrackFrame));
}

void VideoEncoderInstance::StopTrackFrames() {
  is_receiving_track_frames_ = false;
  if (!current_track_frame_.is_null()) {
    video_track_.RecycleFrame(current_track_frame_);
    current_track_frame_.detach();
  }
}

void VideoEncoderInstance::OnTrackFrame(int32_t result, pp::VideoFrame frame) {
  if (result == PP_ERROR_ABORTED)
    return;

  if (!current_track_frame_.is_null()) {
    video_track_.RecycleFrame(current_track_frame_);
    current_track_frame_.detach();
  }

  if (result != PP_OK) {
    LogError(result, "Cannot get video frame from video track");
    return;
  }

  current_track_frame_ = frame;
  if (is_receiving_track_frames_)
    video_track_.GetFrame(callback_factory_.NewCallbackWithOutput(
        &VideoEncoderInstance::OnTrackFrame));
}

void VideoEncoderInstance::StopEncode() {
  video_encoder_.Close();
  StopTrackFrames();
  video_track_.Close();
  is_encoding_ = false;
  encoded_frames_ = 0;
}

//

void VideoEncoderInstance::HandleMessage(const pp::Var& var_message) {
  if (!var_message.is_dictionary()) {
    LogToConsole(PP_LOGLEVEL_ERROR, pp::Var("Invalid message!"));
    return;
  }

  pp::VarDictionary dict_message(var_message);
  std::string command = dict_message.Get("command").AsString();

  if (command == "start") {
    requested_size_ = pp::Size(dict_message.Get("width").AsInt(),
                               dict_message.Get("height").AsInt());
    pp::Var var_track = dict_message.Get("track");
    if (!var_track.is_resource()) {
      LogToConsole(PP_LOGLEVEL_ERROR, pp::Var("Given track is not a resource"));
      return;
    }
    pp::Resource resource_track = var_track.AsResource();
    video_track_ = pp::MediaStreamVideoTrack(resource_track);
    video_encoder_ = pp::VideoEncoder();
    ConfigureTrack();
  } else if (command == "stop") {
    StopEncode();
    PostSignalMessage("stopped");
  } else {
    LogToConsole(PP_LOGLEVEL_ERROR, pp::Var("Invalid command!"));
  }
}

void VideoEncoderInstance::PostDataMessage(const void* buffer, uint32_t size) {
  pp::VarDictionary dictionary;

  dictionary.Set(pp::Var("name"), pp::Var("data"));

  pp::VarArrayBuffer array_buffer(size);
  void* data_ptr = array_buffer.Map();
  memcpy(data_ptr, buffer, size);
  array_buffer.Unmap();
  dictionary.Set(pp::Var("data"), array_buffer);

  PostMessage(dictionary);
}

void VideoEncoderInstance::PostSignalMessage(const char* name) {
  pp::VarDictionary dictionary;
  dictionary.Set(pp::Var("name"), pp::Var(name));

  PostMessage(dictionary);
}

void VideoEncoderInstance::LogError(int32_t error, const std::string& message) {
  std::string msg("Error: ");
  msg.append(pp::Var(error).DebugString());
  msg.append(" : ");
  msg.append(message);
  LogToConsole(PP_LOGLEVEL_ERROR, pp::Var(msg));
}

void VideoEncoderInstance::Log(const std::string& message) {
  LogToConsole(PP_LOGLEVEL_LOG, pp::Var(message));
}

pp::Instance* VideoEncoderModule::CreateInstance(PP_Instance instance) {
  return new VideoEncoderInstance(instance, this);
}

}  // anonymous namespace

namespace pp {
// Factory function for your specialization of the Module object.
Module* CreateModule() {
  return new VideoEncoderModule();
}
}  // namespace pp
