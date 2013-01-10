// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_view.h"

#include <functional>

#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/synchronization/waitable_event.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/dev/graphics_2d_dev.h"
#include "ppapi/cpp/dev/view_dev.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/size.h"
#include "remoting/base/util.h"
#include "remoting/client/chromoting_stats.h"
#include "remoting/client/client_context.h"
#include "remoting/client/frame_producer.h"
#include "remoting/client/plugin/chromoting_instance.h"
#include "remoting/client/plugin/pepper_util.h"

using base::Passed;

namespace remoting {

namespace {

// The maximum number of image buffers to be allocated at any point of time.
const size_t kMaxPendingBuffersCount = 2;

}  // namespace

PepperView::PepperView(ChromotingInstance* instance,
                       ClientContext* context,
                       FrameProducer* producer)
  : instance_(instance),
    context_(context),
    producer_(producer),
    merge_buffer_(NULL),
    merge_clip_area_(SkIRect::MakeEmpty()),
    dips_size_(SkISize::Make(0, 0)),
    dips_to_device_scale_(1.0f),
    view_size_(SkISize::Make(0, 0)),
    dips_to_view_scale_(1.0f),
    clip_area_(SkIRect::MakeEmpty()),
    source_size_(SkISize::Make(0, 0)),
    source_dpi_(SkIPoint::Make(0, 0)),
    flush_pending_(false),
    is_initialized_(false),
    frame_received_(false) {
  InitiateDrawing();
}

PepperView::~PepperView() {
  // The producer should now return any pending buffers. At this point, however,
  // ReturnBuffer() tasks scheduled by the producer will not be delivered,
  // so we free all the buffers once the producer's queue is empty.
  base::WaitableEvent done_event(true, false);
  producer_->RequestReturnBuffers(
      base::Bind(&base::WaitableEvent::Signal, base::Unretained(&done_event)));
  done_event.Wait();

  merge_buffer_ = NULL;
  while (!buffers_.empty()) {
    FreeBuffer(buffers_.front());
  }
}

void PepperView::SetView(const pp::View& view) {
  bool view_changed = false;

  pp::Rect pp_size = view.GetRect();
  SkISize new_dips_size = SkISize::Make(pp_size.width(), pp_size.height());
  pp::ViewDev view_dev(view);
  float new_dips_to_device_scale = view_dev.GetDeviceScale();

  if (dips_size_ != new_dips_size ||
      dips_to_device_scale_ != new_dips_to_device_scale) {
    view_changed = true;
    dips_to_device_scale_ = new_dips_to_device_scale;
    dips_size_ = new_dips_size;

    // If |dips_to_device_scale_| is > 1.0 then the device is high-DPI, and
    // there are actually |view_device_scale_| physical pixels for every one
    // Density Independent Pixel (DIP).  If we specify a scale of 1.0 to
    // Graphics2D then we can render at DIP resolution and let PPAPI up-scale
    // for high-DPI devices.
    dips_to_view_scale_ = 1.0f;
    view_size_ = dips_size_;

    // If the view's DIP dimensions don't match the source then let the frame
    // producer do the scaling, and render at device resolution.
    if (dips_size_ != source_size_) {
      dips_to_view_scale_ = dips_to_device_scale_;
      view_size_ = SkISize::Make(
          ceilf(dips_size_.width() * dips_to_view_scale_),
          ceilf(dips_size_.height() * dips_to_view_scale_));
    }

    // Create a 2D rendering context at the chosen frame dimensions.
    pp::Size pp_size = pp::Size(view_size_.width(), view_size_.height());
    graphics2d_ = pp::Graphics2D(instance_, pp_size, true);

    // Specify the scale from our coordinates to DIPs.
    pp::Graphics2D_Dev graphics2d_dev(graphics2d_);
    graphics2d_dev.SetScale(1.0f / dips_to_view_scale_);

    bool result = instance_->BindGraphics(graphics2d_);

    // There is no good way to handle this error currently.
    DCHECK(result) << "Couldn't bind the device context.";
  }

  pp::Rect pp_clip = view.GetClipRect();
  SkIRect new_clip = SkIRect::MakeLTRB(
      floorf(pp_clip.x() * dips_to_view_scale_),
      floorf(pp_clip.y() * dips_to_view_scale_),
      ceilf(pp_clip.right() * dips_to_view_scale_),
      ceilf(pp_clip.bottom() * dips_to_view_scale_));
  if (clip_area_ != new_clip) {
    view_changed = true;

    // YUV to RGB conversion may require even X and Y coordinates for
    // the top left corner of the clipping area.
    clip_area_ = AlignRect(new_clip);
    clip_area_.intersect(SkIRect::MakeSize(view_size_));
  }

  if (view_changed) {
    producer_->SetOutputSizeAndClip(view_size_, clip_area_);
    InitiateDrawing();
  }
}

void PepperView::ApplyBuffer(const SkISize& view_size,
                             const SkIRect& clip_area,
                             pp::ImageData* buffer,
                             const SkRegion& region) {
  DCHECK(context_->main_task_runner()->BelongsToCurrentThread());

  if (!frame_received_) {
    instance_->OnFirstFrameReceived();
    frame_received_ = true;
  }
  // We cannot use the data in the buffer if its dimensions don't match the
  // current view size.
  // TODO(alexeypa): We could rescale and draw it (or even draw it without
  // rescaling) to reduce the perceived lag while we are waiting for
  // the properly scaled data.
  if (view_size_ != view_size) {
    FreeBuffer(buffer);
    InitiateDrawing();
  } else {
    FlushBuffer(clip_area, buffer, region);
  }
}

void PepperView::ReturnBuffer(pp::ImageData* buffer) {
  DCHECK(context_->main_task_runner()->BelongsToCurrentThread());

  // Reuse the buffer if it is large enough, otherwise drop it on the floor
  // and allocate a new one.
  if (buffer->size().width() >= clip_area_.width() &&
      buffer->size().height() >= clip_area_.height()) {
    producer_->DrawBuffer(buffer);
  } else {
    FreeBuffer(buffer);
    InitiateDrawing();
  }
}

void PepperView::SetSourceSize(const SkISize& source_size,
                               const SkIPoint& source_dpi) {
  DCHECK(context_->main_task_runner()->BelongsToCurrentThread());

  if (source_size_ == source_size && source_dpi_ == source_dpi)
    return;

  source_size_ = source_size;
  source_dpi_ = source_dpi;

  // Notify JavaScript of the change in source size.
  instance_->SetDesktopSize(source_size, source_dpi);
}

pp::ImageData* PepperView::AllocateBuffer() {
  if (buffers_.size() >= kMaxPendingBuffersCount)
    return NULL;

  pp::Size pp_size = pp::Size(clip_area_.width(), clip_area_.height());
  if (pp_size.IsEmpty())
    return NULL;

  // Create an image buffer of the required size, but don't zero it.
  pp::ImageData* buffer =  new pp::ImageData(
      instance_, PP_IMAGEDATAFORMAT_BGRA_PREMUL, pp_size, false);
  if (buffer->is_null()) {
    LOG(WARNING) << "Not enough memory for frame buffers.";
    delete buffer;
    return NULL;
  }

  buffers_.push_back(buffer);
  return buffer;
}

void PepperView::FreeBuffer(pp::ImageData* buffer) {
  DCHECK(std::find(buffers_.begin(), buffers_.end(), buffer) != buffers_.end());

  buffers_.remove(buffer);
  delete buffer;
}

void PepperView::InitiateDrawing() {
  pp::ImageData* buffer = AllocateBuffer();
  while (buffer) {
    producer_->DrawBuffer(buffer);
    buffer = AllocateBuffer();
  }
}

void PepperView::FlushBuffer(const SkIRect& clip_area,
                             pp::ImageData* buffer,
                             const SkRegion& region) {
  // Defer drawing if the flush is already in progress.
  if (flush_pending_) {
    // |merge_buffer_| is guaranteed to be free here because we allocate only
    // two buffers simultaneously. If more buffers are allowed this code should
    // apply all pending changes to the screen.
    DCHECK(merge_buffer_ == NULL);

    merge_clip_area_ = clip_area;
    merge_buffer_ = buffer;
    merge_region_ = region;
    return;
  }

  // Notify Pepper API about the updated areas and flush pixels to the screen.
  base::Time start_time = base::Time::Now();

  for (SkRegion::Iterator i(region); !i.done(); i.next()) {
    SkIRect rect = i.rect();

    // Re-clip |region| with the current clipping area |clip_area_| because
    // the latter could change from the time the buffer was drawn.
    if (!rect.intersect(clip_area_))
      continue;

    // Specify the rectangle coordinates relative to the clipping area.
    rect.offset(-clip_area.left(), -clip_area.top());

    // Pepper Graphics 2D has a strange and badly documented API that the
    // point here is the offset from the source rect. Why?
    graphics2d_.PaintImageData(
        *buffer,
        pp::Point(clip_area.left(), clip_area.top()),
        pp::Rect(rect.left(), rect.top(), rect.width(), rect.height()));
  }

  // Notify the producer that some parts of the region weren't painted because
  // the clipping area has changed already.
  if (clip_area != clip_area_) {
    SkRegion not_painted = region;
    not_painted.op(clip_area_, SkRegion::kDifference_Op);
    if (!not_painted.isEmpty()) {
      producer_->InvalidateRegion(not_painted);
    }
  }

  // Flush the updated areas to the screen.
  int error = graphics2d_.Flush(
      PpCompletionCallback(base::Bind(
          &PepperView::OnFlushDone, AsWeakPtr(), start_time, buffer)));
  CHECK(error == PP_OK_COMPLETIONPENDING);
  flush_pending_ = true;
}

void PepperView::OnFlushDone(base::Time paint_start,
                             pp::ImageData* buffer,
                             int result) {
  DCHECK(context_->main_task_runner()->BelongsToCurrentThread());
  DCHECK(flush_pending_);

  instance_->GetStats()->video_paint_ms()->Record(
      (base::Time::Now() - paint_start).InMilliseconds());

  flush_pending_ = false;
  ReturnBuffer(buffer);

  // If there is a buffer queued for rendering then render it now.
  if (merge_buffer_ != NULL) {
    buffer = merge_buffer_;
    merge_buffer_ = NULL;
    FlushBuffer(merge_clip_area_, buffer, merge_region_);
  }
}

}  // namespace remoting
