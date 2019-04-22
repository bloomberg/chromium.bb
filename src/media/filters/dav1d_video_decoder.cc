// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/dav1d_video_decoder.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_log.h"
#include "media/base/video_util.h"

extern "C" {
#include "third_party/dav1d/libdav1d/include/dav1d/dav1d.h"
}

namespace media {

// Returns the number of threads.
static int GetDecoderThreadCount(const VideoDecoderConfig& config) {
  // For AV1 decode when using the default thread count, increase the number
  // of decode threads to equal the maximum number of tiles possible for
  // higher resolution streams.
  return VideoDecoder::GetRecommendedThreadCount(config.coded_size().width() /
                                                 256);
}

static VideoPixelFormat Dav1dImgFmtToVideoPixelFormat(
    const Dav1dPictureParameters* pic) {
  switch (pic->layout) {
    case DAV1D_PIXEL_LAYOUT_I420:
      switch (pic->bpc) {
        case 8:
          return PIXEL_FORMAT_I420;
        case 10:
          return PIXEL_FORMAT_YUV420P10;
        case 12:
          return PIXEL_FORMAT_YUV420P12;
        default:
          DLOG(ERROR) << "Unsupported bit depth: " << pic->bpc;
          return PIXEL_FORMAT_UNKNOWN;
      }
    case DAV1D_PIXEL_LAYOUT_I422:
      switch (pic->bpc) {
        case 8:
          return PIXEL_FORMAT_I422;
        case 10:
          return PIXEL_FORMAT_YUV422P10;
        case 12:
          return PIXEL_FORMAT_YUV422P12;
        default:
          DLOG(ERROR) << "Unsupported bit depth: " << pic->bpc;
          return PIXEL_FORMAT_UNKNOWN;
      }
    case DAV1D_PIXEL_LAYOUT_I444:
      switch (pic->bpc) {
        case 8:
          return PIXEL_FORMAT_I444;
        case 10:
          return PIXEL_FORMAT_YUV444P10;
        case 12:
          return PIXEL_FORMAT_YUV444P12;
        default:
          DLOG(ERROR) << "Unsupported bit depth: " << pic->bpc;
          return PIXEL_FORMAT_UNKNOWN;
      }
    default:
      DLOG(ERROR) << "Unsupported pixel format: " << pic->layout;
      return PIXEL_FORMAT_UNKNOWN;
  }
}

static void ReleaseDecoderBuffer(const uint8_t* buffer, void* opaque) {
  if (opaque)
    static_cast<DecoderBuffer*>(opaque)->Release();
}

static void LogDav1dMessage(void* cookie, const char* format, va_list ap) {
  auto log = base::StringPrintV(format, ap);
  if (log.empty())
    return;

  if (log.back() == '\n')
    log.pop_back();

  DLOG(ERROR) << log;
}

// std::unique_ptr release helpers. We need to release both the containing
// structs as well as refs held within the structures.
struct ScopedDav1dDataFree {
  void operator()(void* x) const {
    auto* data = static_cast<Dav1dData*>(x);
    dav1d_data_unref(data);
    delete data;
  }
};

struct ScopedDav1dPictureFree {
  void operator()(void* x) const {
    auto* pic = static_cast<Dav1dPicture*>(x);
    dav1d_picture_unref(pic);
    delete pic;
  }
};

Dav1dVideoDecoder::Dav1dVideoDecoder(MediaLog* media_log)
    : media_log_(media_log) {
  DETACH_FROM_THREAD(thread_checker_);
}

Dav1dVideoDecoder::~Dav1dVideoDecoder() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CloseDecoder();
}

std::string Dav1dVideoDecoder::GetDisplayName() const {
  return "Dav1dVideoDecoder";
}

void Dav1dVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                   bool low_delay,
                                   CdmContext* /* cdm_context */,
                                   const InitCB& init_cb,
                                   const OutputCB& output_cb,
                                   const WaitingCB& /* waiting_cb */) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(config.IsValidConfig());

  InitCB bound_init_cb = BindToCurrentLoop(init_cb);
  if (config.is_encrypted() || config.codec() != kCodecAV1) {
    bound_init_cb.Run(false);
    return;
  }

  // Clear any previously initialized decoder.
  CloseDecoder();

  Dav1dSettings s;
  dav1d_default_settings(&s);
  s.n_tile_threads = GetDecoderThreadCount(config);

  // Use only 1 frame thread in low delay mode, otherwise we'll require at least
  // two buffers before the first frame can be output.
  s.n_frame_threads = low_delay ? 1 : 2;

  // Route dav1d internal logs through Chrome's DLOG system.
  s.logger = {nullptr, &LogDav1dMessage};

  if (dav1d_open(&dav1d_decoder_, &s) < 0) {
    bound_init_cb.Run(false);
    return;
  }

  config_ = config;
  state_ = DecoderState::kNormal;
  output_cb_ = BindToCurrentLoop(output_cb);
  bound_init_cb.Run(true);
}

void Dav1dVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                               const DecodeCB& decode_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(buffer);
  DCHECK(decode_cb);
  DCHECK_NE(state_, DecoderState::kUninitialized)
      << "Called Decode() before successful Initialize()";

  DecodeCB bound_decode_cb = BindToCurrentLoop(decode_cb);

  if (state_ == DecoderState::kError) {
    bound_decode_cb.Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  if (!DecodeBuffer(std::move(buffer))) {
    state_ = DecoderState::kError;
    bound_decode_cb.Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  // VideoDecoderShim expects |decode_cb| call after |output_cb_|.
  bound_decode_cb.Run(DecodeStatus::OK);
}

void Dav1dVideoDecoder::Reset(const base::RepeatingClosure& reset_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  state_ = DecoderState::kNormal;
  dav1d_flush(dav1d_decoder_);
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE, reset_cb);
}

void Dav1dVideoDecoder::CloseDecoder() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!dav1d_decoder_)
    return;
  dav1d_close(&dav1d_decoder_);
  DCHECK(!dav1d_decoder_);
}

bool Dav1dVideoDecoder::DecodeBuffer(scoped_refptr<DecoderBuffer> buffer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  using ScopedPtrDav1dData = std::unique_ptr<Dav1dData, ScopedDav1dDataFree>;
  ScopedPtrDav1dData input_buffer;

  if (!buffer->end_of_stream()) {
    input_buffer.reset(new Dav1dData{0});
    if (dav1d_data_wrap(input_buffer.get(), buffer->data(), buffer->data_size(),
                        &ReleaseDecoderBuffer, buffer.get()) < 0) {
      return false;
    }
    input_buffer->m.timestamp = buffer->timestamp().InMicroseconds();
    buffer->AddRef();
  }

  // Used to DCHECK that dav1d_send_data() actually takes the packet. If we exit
  // this function without sending |input_buffer| that packet will be lost. We
  // have no packet to send at end of stream.
  bool send_data_completed = buffer->end_of_stream();

  while (!input_buffer || input_buffer->sz) {
    if (input_buffer) {
      const int res = dav1d_send_data(dav1d_decoder_, input_buffer.get());
      if (res < 0 && res != -EAGAIN) {
        MEDIA_LOG(ERROR, media_log_) << "dav1d_send_data() failed on "
                                     << buffer->AsHumanReadableString();
        return false;
      }

      if (res != -EAGAIN)
        send_data_completed = true;

      // Even if dav1d_send_data() returned EAGAIN, try dav1d_get_picture().
    }

    using ScopedPtrDav1dPicture =
        std::unique_ptr<Dav1dPicture, ScopedDav1dPictureFree>;
    ScopedPtrDav1dPicture p(new Dav1dPicture{0});

    const int res = dav1d_get_picture(dav1d_decoder_, p.get());
    if (res < 0) {
      if (res != -EAGAIN) {
        MEDIA_LOG(ERROR, media_log_) << "dav1d_get_picture() failed on "
                                     << buffer->AsHumanReadableString();
        return false;
      }

      // We've reached end of stream and no frames remain to drain.
      if (!input_buffer) {
        DCHECK(send_data_completed);
        return true;
      }

      continue;
    }

    auto frame = CopyImageToVideoFrame(p.get());
    if (!frame) {
      MEDIA_LOG(DEBUG, media_log_)
          << "Failed to produce video frame from Dav1dPicture.";
      return false;
    }

    // AV1 color space defines match ISO 23001-8:2016 via ISO/IEC 23091-4/ITU-T
    // H.273. https://aomediacodec.github.io/av1-spec/#color-config-semantics
    media::VideoColorSpace color_space(
        p->seq_hdr->pri, p->seq_hdr->trc, p->seq_hdr->mtrx,
        p->seq_hdr->color_range ? gfx::ColorSpace::RangeID::FULL
                                : gfx::ColorSpace::RangeID::LIMITED);

    // If the frame doesn't specify a color space, use the container's.
    if (!color_space.IsSpecified())
      color_space = config_.color_space_info();

    frame->set_color_space(color_space.ToGfxColorSpace());
    frame->metadata()->SetBoolean(VideoFrameMetadata::POWER_EFFICIENT, false);
    frame->AddDestructionObserver(base::BindOnce(
        base::DoNothing::Once<ScopedPtrDav1dPicture>(), std::move(p)));

    output_cb_.Run(std::move(frame));
  }

  DCHECK(send_data_completed);
  return true;
}

scoped_refptr<VideoFrame> Dav1dVideoDecoder::CopyImageToVideoFrame(
    const Dav1dPicture* pic) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  VideoPixelFormat pixel_format = Dav1dImgFmtToVideoPixelFormat(&pic->p);
  if (pixel_format == PIXEL_FORMAT_UNKNOWN)
    return nullptr;

  // Since we're making a copy, only copy the visible area.
  const gfx::Size visible_size(pic->p.w, pic->p.h);
  return VideoFrame::WrapExternalYuvData(
      pixel_format, visible_size, gfx::Rect(visible_size),
      config_.natural_size(), pic->stride[0], pic->stride[1], pic->stride[1],
      static_cast<uint8_t*>(pic->data[0]), static_cast<uint8_t*>(pic->data[1]),
      static_cast<uint8_t*>(pic->data[2]),
      base::TimeDelta::FromMicroseconds(pic->m.timestamp));
}

}  // namespace media
