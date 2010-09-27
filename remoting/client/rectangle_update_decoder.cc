// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/rectangle_update_decoder.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "media/base/callback.h"
#include "remoting/base/decoder.h"
#include "remoting/base/decoder_verbatim.h"
#include "remoting/base/decoder_zlib.h"
#include "remoting/base/protocol/chromotocol.pb.h"
#include "remoting/base/tracer.h"
#include "remoting/client/frame_consumer.h"

using media::AutoTaskRunner;

namespace remoting {

namespace {

class PartialFrameCleanup : public Task {
 public:
  PartialFrameCleanup(media::VideoFrame* frame, UpdatedRects* rects)
      : frame_(frame), rects_(rects) {
  }

  virtual void Run() {
    delete rects_;
    frame_ = NULL;
  }

 private:
  scoped_refptr<media::VideoFrame> frame_;
  UpdatedRects* rects_;
};

}  // namespace

RectangleUpdateDecoder::RectangleUpdateDecoder(MessageLoop* message_loop,
                                               FrameConsumer* consumer)
    : message_loop_(message_loop),
      consumer_(consumer) {
}

RectangleUpdateDecoder::~RectangleUpdateDecoder() {
}

void RectangleUpdateDecoder::DecodePacket(const RectangleUpdatePacket& packet,
                                          Task* done) {
  if (message_loop_ != MessageLoop::current()) {
    message_loop_->PostTask(
        FROM_HERE,
        NewTracedMethod(this,
                        &RectangleUpdateDecoder::DecodePacket, packet,
                        done));
    return;
  }
  AutoTaskRunner done_runner(done);

  TraceContext::tracer()->PrintString("Decode Packet called.");

  if (!IsValidPacket(packet)) {
    LOG(ERROR) << "Received invalid packet.";
    return;
  }

  Task* process_packet_data =
      NewTracedMethod(this,
                      &RectangleUpdateDecoder::ProcessPacketData,
                      packet, done_runner.release());

  if (packet.flags() | RectangleUpdatePacket::FIRST_PACKET) {
    const RectangleFormat& format = packet.format();

    InitializeDecoder(format, process_packet_data);
  } else {
    process_packet_data->Run();
    delete process_packet_data;
  }
}

void RectangleUpdateDecoder::ProcessPacketData(
    const RectangleUpdatePacket& packet,
    Task* done) {
  AutoTaskRunner done_runner(done);

  if (!decoder_->IsReadyForData()) {
    // TODO(ajwong): This whole thing should move into an invalid state.
    LOG(ERROR) << "Decoder is unable to process data. Dropping packet.";
    return;
  }

  TraceContext::tracer()->PrintString("Executing Decode.");
  decoder_->DecodeBytes(packet.encoded_rect());

  if (packet.flags() | RectangleUpdatePacket::LAST_PACKET) {
    decoder_->Reset();

    UpdatedRects* rects = new UpdatedRects();

    // Empty out the list of current updated rects so the decoder can keep
    // writing new ones while these are processed.
    rects->swap(updated_rects_);

    consumer_->OnPartialFrameOutput(frame_, rects,
                                    new PartialFrameCleanup(frame_, rects));
  }
}

// static
bool RectangleUpdateDecoder::IsValidPacket(
    const RectangleUpdatePacket& packet) {
  if (!packet.IsValid()) {
    LOG(WARNING) << "Protobuf consistency checks fail.";
    return false;
  }

  // First packet must have a format.
  if (packet.flags() | RectangleUpdatePacket::FIRST_PACKET) {
    if (!packet.has_format()) {
      LOG(WARNING) << "First packet must have format.";
      return false;
    }

    const RectangleFormat& format = packet.format();
    if (!format.has_encoding() ||
        format.encoding() == EncodingInvalid) {
      LOG(WARNING) << "Invalid encoding specified.";
      return false;
    }
  }

  // We shouldn't generate null packets.
  if (!packet.has_encoded_rect()) {
    LOG(WARNING) << "Packet w/o an encoded rectangle received.";
    return false;
  }

  return true;
}

void RectangleUpdateDecoder::InitializeDecoder(const RectangleFormat& format,
                                               Task* done) {
  if (message_loop_ != MessageLoop::current()) {
    message_loop_->PostTask(
        FROM_HERE,
        NewTracedMethod(this,
                        &RectangleUpdateDecoder::InitializeDecoder,
                        format, done));
    return;
  }
  AutoTaskRunner done_runner(done);

  // Check if we need to request a new frame.
  if (!frame_ ||
      frame_->width() < static_cast<size_t>(format.width()) ||
      frame_->height() < static_cast<size_t>(format.height())) {
    if (frame_) {
      TraceContext::tracer()->PrintString("Releasing old frame.");
      consumer_->ReleaseFrame(frame_);
      frame_ = NULL;
    }
    TraceContext::tracer()->PrintString("Requesting new frame.");
    consumer_->AllocateFrame(media::VideoFrame::RGB32,
                             format.width(), format.height(),
                             base::TimeDelta(), base::TimeDelta(),
                             &frame_,
                             NewTracedMethod(
                                 this,
                                 &RectangleUpdateDecoder::InitializeDecoder,
                                 format,
                                 done_runner.release()));
    return;
  }

  // TODO(ajwong): We need to handle the allocator failing to create a frame
  // and properly disable this class.
  CHECK(frame_);

  if (decoder_.get()) {
    // TODO(ajwong): We need to handle changing decoders midstream correctly
    // since someone may feed us a corrupt stream, or manually create a
    // stream that changes decoders. At the very leask, we should not
    // crash the process.
    //
    // For now though, assume that only one encoding is used throughout.
    //
    // Note, this may be as simple as just deleting the current decoder.
    // However, we need to verify the flushing semantics of the decoder first.
    CHECK(decoder_->Encoding() == format.encoding());
  } else {
    // Initialize a new decoder based on this message encoding.
    if (format.encoding() == EncodingNone) {
      TraceContext::tracer()->PrintString("Creating Verbatim decoder.");
      decoder_.reset(new DecoderVerbatim());
    } else if (format.encoding() == EncodingZlib) {
      TraceContext::tracer()->PrintString("Creating Zlib decoder");
      decoder_.reset(new DecoderZlib());
    } else {
      NOTREACHED << "Invalid Encoding found: " << found.encoding();
    }
  }

  // TODO(ajwong): This can happen in the face of corrupt input data.  Figure
  // out the right behavior and make this more resilient.
  CHECK(updated_rects_.empty());

  gfx::Rect rectangle_size(format.x(), format.y(),
                           format.width(), format.height());
  updated_rects_.push_back(rectangle_size);
  decoder_->Initialize(frame_, rectangle_size);
  TraceContext::tracer()->PrintString("Decoder is Initialized");
}

}  // namespace remoting
