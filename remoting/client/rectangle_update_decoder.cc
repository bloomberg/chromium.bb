// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/rectangle_update_decoder.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "ppapi/cpp/image_data.h"
#include "remoting/base/decoder.h"
#include "remoting/base/decoder_row_based.h"
#include "remoting/base/decoder_vp8.h"
#include "remoting/base/util.h"
#include "remoting/client/frame_consumer.h"
#include "remoting/protocol/session_config.h"

using base::Passed;
using remoting::protocol::ChannelConfig;
using remoting::protocol::SessionConfig;

namespace remoting {

RectangleUpdateDecoder::RectangleUpdateDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    scoped_refptr<FrameConsumerProxy> consumer)
    : task_runner_(task_runner),
      consumer_(consumer),
      source_size_(SkISize::Make(0, 0)),
      view_size_(SkISize::Make(0, 0)),
      clip_area_(SkIRect::MakeEmpty()),
      paint_scheduled_(false) {
}

RectangleUpdateDecoder::~RectangleUpdateDecoder() {
}

void RectangleUpdateDecoder::Initialize(const SessionConfig& config) {
  // Initialize decoder based on the selected codec.
  ChannelConfig::Codec codec = config.video_config().codec;
  if (codec == ChannelConfig::CODEC_VERBATIM) {
    decoder_.reset(DecoderRowBased::CreateVerbatimDecoder());
  } else if (codec == ChannelConfig::CODEC_ZIP) {
    decoder_.reset(DecoderRowBased::CreateZlibDecoder());
  } else if (codec == ChannelConfig::CODEC_VP8) {
    decoder_.reset(new DecoderVp8());
  } else {
    NOTREACHED() << "Invalid Encoding found: " << codec;
  }
}

void RectangleUpdateDecoder::DecodePacket(scoped_ptr<VideoPacket> packet,
                                          const base::Closure& done) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&RectangleUpdateDecoder::DecodePacket,
                              this, base::Passed(&packet), done));
    return;
  }

  base::ScopedClosureRunner done_runner(done);
  bool decoder_needs_reset = false;

  // If the packet includes a screen size, store it.
  if (packet->format().has_screen_width() &&
      packet->format().has_screen_height()) {
    SkISize source_size = SkISize::Make(packet->format().screen_width(),
                                        packet->format().screen_height());
    if (source_size_ != source_size) {
      source_size_ = source_size;
      decoder_needs_reset = true;
    }
  }

  // If we've never seen a screen size, ignore the packet.
  if (source_size_.isZero()) {
    return;
  }

  if (decoder_needs_reset) {
    decoder_->Initialize(source_size_);
    consumer_->SetSourceSize(source_size_);
  }

  if (!decoder_->IsReadyForData()) {
    // TODO(ajwong): This whole thing should move into an invalid state.
    LOG(ERROR) << "Decoder is unable to process data. Dropping packet.";
    return;
  }

  if (decoder_->DecodePacket(packet.get()) == Decoder::DECODE_DONE)
    SchedulePaint();
}

void RectangleUpdateDecoder::SchedulePaint() {
  if (paint_scheduled_)
    return;
  paint_scheduled_ = true;
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&RectangleUpdateDecoder::DoPaint, this));
}

void RectangleUpdateDecoder::DoPaint() {
  DCHECK(paint_scheduled_);
  paint_scheduled_ = false;

  // If the view size is empty or we have no output buffers ready, return.
  if (buffers_.empty() || view_size_.isEmpty())
    return;

  // If no Decoder is initialized, or the host dimensions are empty, return.
  if (!decoder_.get() || source_size_.isEmpty())
    return;

  // Draw the invalidated region to the buffer.
  pp::ImageData* buffer = buffers_.front();
  SkRegion output_region;
  decoder_->RenderFrame(view_size_, clip_area_,
                        reinterpret_cast<uint8*>(buffer->data()),
                        buffer->stride(),
                        &output_region);

  // Notify the consumer that painting is done.
  if (!output_region.isEmpty()) {
    buffers_.pop_front();
    consumer_->ApplyBuffer(view_size_, clip_area_, buffer, output_region);
  }
}

void RectangleUpdateDecoder::RequestReturnBuffers(const base::Closure& done) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&RectangleUpdateDecoder::RequestReturnBuffers,
        this, done));
    return;
  }

  while (!buffers_.empty()) {
    consumer_->ReturnBuffer(buffers_.front());
    buffers_.pop_front();
  }

  if (!done.is_null())
    done.Run();
}

void RectangleUpdateDecoder::DrawBuffer(pp::ImageData* buffer) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&RectangleUpdateDecoder::DrawBuffer,
                              this, buffer));
    return;
  }

  DCHECK(clip_area_.width() <= buffer->size().width() &&
         clip_area_.height() <= buffer->size().height());

  buffers_.push_back(buffer);
  SchedulePaint();
}

void RectangleUpdateDecoder::InvalidateRegion(const SkRegion& region) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&RectangleUpdateDecoder::InvalidateRegion,
                              this, region));
    return;
  }

  if (decoder_.get()) {
    decoder_->Invalidate(view_size_, region);
    SchedulePaint();
  }
}

void RectangleUpdateDecoder::SetOutputSizeAndClip(const SkISize& view_size,
                                                  const SkIRect& clip_area) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&RectangleUpdateDecoder::SetOutputSizeAndClip,
                              this, view_size, clip_area));
    return;
  }

  // The whole frame needs to be repainted if the scaling factor has changed.
  if (view_size_ != view_size && decoder_.get()) {
    SkRegion region;
    region.op(SkIRect::MakeSize(view_size), SkRegion::kUnion_Op);
    decoder_->Invalidate(view_size, region);
  }

  if (view_size_ != view_size ||
      clip_area_ != clip_area) {
    view_size_ = view_size;
    clip_area_ = clip_area;

    // Return buffers that are smaller than needed to the consumer for
    // reuse/reallocation.
    std::list<pp::ImageData*>::iterator i = buffers_.begin();
    while (i != buffers_.end()) {
      pp::Size buffer_size = (*i)->size();
      if (buffer_size.width() < clip_area_.width() ||
          buffer_size.height() < clip_area_.height()) {
        consumer_->ReturnBuffer(*i);
        i = buffers_.erase(i);
      } else {
        ++i;
      }
    }

    SchedulePaint();
  }
}

}  // namespace remoting
