// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/video_frame_recorder_host_extension.h"

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "remoting/codec/video_encoder_verbatim.h"
#include "remoting/host/host_extension_session.h"
#include "remoting/host/video_frame_recorder.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/client_stub.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {

namespace {

const char kVideoRecorderCapabilities[] = "videoRecorder";

const char kVideoRecorderType[] = "video-recorder";

const char kType[] = "type";
const char kData[] = "data";

const char kStartType[] = "start";
const char kStopType[] = "stop";
const char kNextFrameType[] = "next-frame";
const char kNextFrameReplyType[] = "next-frame-reply";

class VideoFrameRecorderHostExtensionSession : public HostExtensionSession {
 public:
  explicit VideoFrameRecorderHostExtensionSession(int64_t max_content_bytes);
  virtual ~VideoFrameRecorderHostExtensionSession() {}

  // remoting::HostExtensionSession interface.
  virtual scoped_ptr<VideoEncoder> OnCreateVideoEncoder(
      scoped_ptr<VideoEncoder> encoder) OVERRIDE;
  virtual bool ModifiesVideoPipeline() const OVERRIDE;
  virtual bool OnExtensionMessage(
      ClientSessionControl* client_session_control,
      protocol::ClientStub* client_stub,
      const protocol::ExtensionMessage& message) OVERRIDE;

 private:
  VideoEncoderVerbatim verbatim_encoder;
  VideoFrameRecorder video_frame_recorder;
  bool first_frame_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameRecorderHostExtensionSession);
};

VideoFrameRecorderHostExtensionSession::VideoFrameRecorderHostExtensionSession(
    int64_t max_content_bytes) : first_frame_(false) {
  video_frame_recorder.SetMaxContentBytes(max_content_bytes);
}

scoped_ptr<VideoEncoder>
VideoFrameRecorderHostExtensionSession::OnCreateVideoEncoder(
    scoped_ptr<VideoEncoder> encoder) {
  video_frame_recorder.DetachVideoEncoderWrapper();
  return video_frame_recorder.WrapVideoEncoder(encoder.Pass());
}

bool VideoFrameRecorderHostExtensionSession::ModifiesVideoPipeline() const {
  return true;
}

bool VideoFrameRecorderHostExtensionSession::OnExtensionMessage(
    ClientSessionControl* client_session_control,
    protocol::ClientStub* client_stub,
    const protocol::ExtensionMessage& message) {
  if (message.type() != kVideoRecorderType) {
    return false;
  }

  if (!message.has_data()) {
    return true;
  }

  scoped_ptr<base::Value> value(base::JSONReader::Read(message.data()));
  base::DictionaryValue* client_message;
  if (value && value->GetAsDictionary(&client_message)) {
    std::string type;
    if (!client_message->GetString(kType, &type)) {
      LOG(ERROR) << "Invalid video-recorder message";
      return true;
    }

    if (type == kStartType) {
      video_frame_recorder.SetEnableRecording(true);
      first_frame_ = true;
    } else if (type == kStopType) {
      video_frame_recorder.SetEnableRecording(false);
    } else if (type == kNextFrameType) {
      scoped_ptr<webrtc::DesktopFrame> frame(video_frame_recorder.NextFrame());

      // TODO(wez): This involves six copies of the entire frame.
      // See if there's some way to optimize at least a few of them out.
      base::DictionaryValue reply_message;
      reply_message.SetString(kType, kNextFrameReplyType);
      if (frame) {
        // If this is the first frame then override the updated region so that
        // the encoder will send the whole frame's contents.
        if (first_frame_) {
          first_frame_ = false;

          frame->mutable_updated_region()->SetRect(
              webrtc::DesktopRect::MakeSize(frame->size()));
        }

        // Encode the frame into a raw ARGB VideoPacket.
        scoped_ptr<VideoPacket> encoded_frame(
            verbatim_encoder.Encode(*frame));

        // Serialize that packet into a string.
        std::string data;
        data.resize(encoded_frame->ByteSize());
        encoded_frame->SerializeWithCachedSizesToArray(
            reinterpret_cast<uint8_t*>(&data[0]));

        // Convert that string to Base64, so it's JSON-friendly.
        std::string base64_data;
        base::Base64Encode(data, &base64_data);

        // Copy the Base64 data into the message.
        reply_message.SetString(kData, base64_data);
      }

      // JSON-encode the reply into a string.
      std::string reply_json;
      if (!base::JSONWriter::Write(&reply_message, &reply_json)) {
        LOG(ERROR) << "Failed to create reply json";
        return true;
      }

      // Return the frame (or a 'data'-less reply) to the client.
      protocol::ExtensionMessage message;
      message.set_type(kVideoRecorderType);
      message.set_data(reply_json);
      client_stub->DeliverHostMessage(message);
    }
  }

  return true;
}

} // namespace

void VideoFrameRecorderHostExtension::SetMaxContentBytes(
    int64_t max_content_bytes) {
  max_content_bytes_ = max_content_bytes;
}

std::string VideoFrameRecorderHostExtension::capability() const {
  return kVideoRecorderCapabilities;
}

scoped_ptr<HostExtensionSession>
VideoFrameRecorderHostExtension::CreateExtensionSession(
    ClientSessionControl* client_session_control,
    protocol::ClientStub* client_stub) {
  return scoped_ptr<HostExtensionSession>(
      new VideoFrameRecorderHostExtensionSession(max_content_bytes_));
}

}  // namespace remoting
