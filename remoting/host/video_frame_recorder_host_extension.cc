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

// Name of the extension message type field, and its value for this extension.
const char kType[] = "type";
const char kVideoRecorderType[] = "video-recorder";

class VideoFrameRecorderHostExtensionSession : public HostExtensionSession {
 public:
  explicit VideoFrameRecorderHostExtensionSession(int64_t max_content_bytes);
  virtual ~VideoFrameRecorderHostExtensionSession();

  // remoting::HostExtensionSession interface.
  virtual void OnCreateVideoEncoder(scoped_ptr<VideoEncoder>* encoder) OVERRIDE;
  virtual bool ModifiesVideoPipeline() const OVERRIDE;
  virtual bool OnExtensionMessage(
      ClientSessionControl* client_session_control,
      protocol::ClientStub* client_stub,
      const protocol::ExtensionMessage& message) OVERRIDE;

 private:
  // Handlers for the different frame recorder extension message types.
  void OnStart();
  void OnStop();
  void OnNextFrame(protocol::ClientStub* client_stub);

  VideoEncoderVerbatim verbatim_encoder_;
  VideoFrameRecorder video_frame_recorder_;
  bool first_frame_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameRecorderHostExtensionSession);
};

VideoFrameRecorderHostExtensionSession::VideoFrameRecorderHostExtensionSession(
    int64_t max_content_bytes)
    : first_frame_(false) {
  video_frame_recorder_.SetMaxContentBytes(max_content_bytes);
}

VideoFrameRecorderHostExtensionSession::
    ~VideoFrameRecorderHostExtensionSession() {
}

void VideoFrameRecorderHostExtensionSession::OnCreateVideoEncoder(
    scoped_ptr<VideoEncoder>* encoder) {
  video_frame_recorder_.DetachVideoEncoderWrapper();
  *encoder = video_frame_recorder_.WrapVideoEncoder(encoder->Pass());
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
  if (!value || !value->GetAsDictionary(&client_message)) {
    return true;
  }

  std::string type;
  if (!client_message->GetString(kType, &type)) {
    LOG(ERROR) << "Invalid video-recorder message";
    return true;
  }

  const char kStartType[] = "start";
  const char kStopType[] = "stop";
  const char kNextFrameType[] = "next-frame";

  if (type == kStartType) {
    OnStart();
  } else if (type == kStopType) {
    OnStop();
  } else if (type == kNextFrameType) {
    OnNextFrame(client_stub);
  }

  return true;
}

void VideoFrameRecorderHostExtensionSession::OnStart() {
  video_frame_recorder_.SetEnableRecording(true);
  first_frame_ = true;
}

void VideoFrameRecorderHostExtensionSession::OnStop() {
  video_frame_recorder_.SetEnableRecording(false);
}

void VideoFrameRecorderHostExtensionSession::OnNextFrame(
    protocol::ClientStub* client_stub) {
  scoped_ptr<webrtc::DesktopFrame> frame(video_frame_recorder_.NextFrame());

  // TODO(wez): This involves six copies of the entire frame.
  // See if there's some way to optimize at least a few of them out.
  const char kNextFrameReplyType[] = "next-frame-reply";
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
        verbatim_encoder_.Encode(*frame));

    // Serialize that packet into a string.
    std::string data(encoded_frame->ByteSize(), 0);
    encoded_frame->SerializeWithCachedSizesToArray(
        reinterpret_cast<uint8_t*>(&data[0]));

    // Convert that string to Base64, so it's JSON-friendly.
    std::string base64_data;
    base::Base64Encode(data, &base64_data);

    // Copy the Base64 data into the message.
    const char kData[] = "data";
    reply_message.SetString(kData, base64_data);
  }

  // JSON-encode the reply into a string.
  // Note that JSONWriter::Write() can only fail due to invalid inputs, and will
  // DCHECK in Debug builds in that case.
  std::string reply_json;
  if (!base::JSONWriter::Write(&reply_message, &reply_json)) {
    return;
  }

  // Return the frame (or a 'data'-less reply) to the client.
  protocol::ExtensionMessage message;
  message.set_type(kVideoRecorderType);
  message.set_data(reply_json);
  client_stub->DeliverHostMessage(message);
}

} // namespace

VideoFrameRecorderHostExtension::VideoFrameRecorderHostExtension() {}

VideoFrameRecorderHostExtension::~VideoFrameRecorderHostExtension() {}

void VideoFrameRecorderHostExtension::SetMaxContentBytes(
    int64_t max_content_bytes) {
  max_content_bytes_ = max_content_bytes;
}

std::string VideoFrameRecorderHostExtension::capability() const {
  const char kVideoRecorderCapability[] = "videoRecorder";
  return kVideoRecorderCapability;
}

scoped_ptr<HostExtensionSession>
VideoFrameRecorderHostExtension::CreateExtensionSession(
    ClientSessionControl* client_session_control,
    protocol::ClientStub* client_stub) {
  return scoped_ptr<HostExtensionSession>(
      new VideoFrameRecorderHostExtensionSession(max_content_bytes_));
}

}  // namespace remoting
