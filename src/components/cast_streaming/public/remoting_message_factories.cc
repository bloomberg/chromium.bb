// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/public/remoting_proto_enum_utils.h"
#include "components/cast_streaming/public/remoting_proto_utils.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/buffering_state.h"
#include "media/base/pipeline_status.h"
#include "media/base/video_decoder_config.h"
#include "third_party/openscreen/src/cast/streaming/remoting.pb.h"

namespace cast_streaming {
namespace remoting {
namespace {

std::unique_ptr<openscreen::cast::RpcMessage> CreateMessage(
    openscreen::cast::RpcMessage_RpcProc type) {
  auto rpc = std::make_unique<openscreen::cast::RpcMessage>();
  rpc->set_proc(type);
  return rpc;
}

}  // namespace

std::unique_ptr<openscreen::cast::RpcMessage> CreateMessageForError() {
  return CreateMessage(openscreen::cast::RpcMessage::RPC_RC_ONERROR);
}

std::unique_ptr<openscreen::cast::RpcMessage> CreateMessageForMediaEnded() {
  return CreateMessage(openscreen::cast::RpcMessage::RPC_RC_ONENDED);
}

std::unique_ptr<openscreen::cast::RpcMessage> CreateMessageForStatisticsUpdate(
    const media::PipelineStatistics& stats) {
  auto rpc =
      CreateMessage(openscreen::cast::RpcMessage::RPC_RC_ONSTATISTICSUPDATE);
  auto* message = rpc->mutable_rendererclient_onstatisticsupdate_rpc();
  message->set_audio_bytes_decoded(stats.audio_bytes_decoded);
  message->set_video_bytes_decoded(stats.video_bytes_decoded);
  message->set_video_frames_decoded(stats.video_frames_decoded);
  message->set_video_frames_dropped(stats.video_frames_dropped);
  message->set_audio_memory_usage(stats.audio_memory_usage);
  message->set_video_memory_usage(stats.video_memory_usage);
  return rpc;
}

std::unique_ptr<openscreen::cast::RpcMessage>
CreateMessageForBufferingStateChange(media::BufferingState state) {
  auto rpc = CreateMessage(
      openscreen::cast::RpcMessage::RPC_RC_ONBUFFERINGSTATECHANGE);
  auto* message = rpc->mutable_rendererclient_onbufferingstatechange_rpc();
  message->set_state(ToProtoMediaBufferingState(state).value());
  return rpc;
}

std::unique_ptr<openscreen::cast::RpcMessage> CreateMessageForAudioConfigChange(
    const media::AudioDecoderConfig& config) {
  auto rpc =
      CreateMessage(openscreen::cast::RpcMessage::RPC_RC_ONAUDIOCONFIGCHANGE);
  auto* message = rpc->mutable_rendererclient_onaudioconfigchange_rpc();
  openscreen::cast::AudioDecoderConfig* proto_audio_config =
      message->mutable_audio_decoder_config();
  ConvertAudioDecoderConfigToProto(config, proto_audio_config);
  return rpc;
}

std::unique_ptr<openscreen::cast::RpcMessage> CreateMessageForVideoConfigChange(
    const media::VideoDecoderConfig& config) {
  auto rpc =
      CreateMessage(openscreen::cast::RpcMessage::RPC_RC_ONVIDEOCONFIGCHANGE);
  auto* message = rpc->mutable_rendererclient_onvideoconfigchange_rpc();
  openscreen::cast::VideoDecoderConfig* proto_video_config =
      message->mutable_video_decoder_config();
  ConvertVideoDecoderConfigToProto(config, proto_video_config);
  return rpc;
}

std::unique_ptr<openscreen::cast::RpcMessage>
CreateMessageForVideoNaturalSizeChange(const gfx::Size& size) {
  auto rpc = CreateMessage(
      openscreen::cast::RpcMessage::RPC_RC_ONVIDEONATURALSIZECHANGE);
  auto* message = rpc->mutable_rendererclient_onvideonatualsizechange_rpc();
  message->set_width(size.width());
  message->set_height(size.height());
  return rpc;
}

std::unique_ptr<openscreen::cast::RpcMessage>
CreateMessageForVideoOpacityChange(bool opaque) {
  auto rpc =
      CreateMessage(openscreen::cast::RpcMessage::RPC_RC_ONVIDEOOPACITYCHANGE);
  rpc->set_boolean_value(opaque);
  return rpc;
}

std::unique_ptr<openscreen::cast::RpcMessage> CreateMessageForMediaTimeUpdate(
    base::TimeDelta media_time) {
  auto rpc = CreateMessage(openscreen::cast::RpcMessage::RPC_RC_ONTIMEUPDATE);
  auto* message = rpc->mutable_rendererclient_ontimeupdate_rpc();
  message->set_time_usec(media_time.InMicroseconds());
  message->set_max_time_usec(media_time.InMicroseconds());
  return rpc;
}

std::unique_ptr<openscreen::cast::RpcMessage>
CreateMessageForInitializationComplete(bool has_succeeded) {
  auto rpc =
      CreateMessage(openscreen::cast::RpcMessage::RPC_R_INITIALIZE_CALLBACK);
  rpc->set_boolean_value(has_succeeded);
  return rpc;
}

std::unique_ptr<openscreen::cast::RpcMessage> CreateMessageForFlushComplete() {
  return CreateMessage(openscreen::cast::RpcMessage::RPC_R_FLUSHUNTIL_CALLBACK);
}

}  // namespace remoting
}  // namespace cast_streaming
