// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_view.h"

#include <functional>

#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/size.h"
#include "remoting/base/util.h"
#include "remoting/client/chromoting_stats.h"
#include "remoting/client/client_context.h"
#include "remoting/client/frame_producer.h"
#include "remoting/client/plugin/chromoting_instance.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

using base::Passed;

namespace {

// DesktopFrame that wraps a supplied pp::ImageData
class PepperDesktopFrame : public webrtc::DesktopFrame {
 public:
  // Wraps the supplied ImageData.
  explicit PepperDesktopFrame(const pp::ImageData& buffer);

  // Access to underlying pepper representation.
  const pp::ImageData& buffer() const {
    return buffer_;
  }

 private:
  pp::ImageData buffer_;
};

PepperDesktopFrame::PepperDesktopFrame(const pp::ImageData& buffer)
  : DesktopFrame(webrtc::DesktopSize(buffer.size().width(),
                                     buffer.size().height()),
                 buffer.stride(),
                 reinterpret_cast<uint8_t*>(buffer.data()),
                 NULL),
    buffer_(buffer) {}

}  // namespace

namespace remoting {

namespace {

// The maximum number of image buffers to be allocated at any point of time.
const size_t kMaxPendingBuffersCount = 2;

}  // namespace

PepperView::PepperView(ChromotingInstance* instance,
                       ClientContext* context)
  : instance_(instance),
    context_(context),
    producer_(NULL),
    merge_buffer_(NULL),
    dips_to_device_scale_(1.0f),
    dips_to_view_scale_(1.0f),
    flush_pending_(false),
    is_initialized_(false),
    frame_received_(false),
    callback_factory_(this) {
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

void PepperView::Initialize(FrameProducer* producer) {
  producer_ = producer;
  webrtc::DesktopFrame* buffer = AllocateBuffer();
  while (buffer) {
    producer_->DrawBuffer(buffer);
    buffer = AllocateBuffer();
  }
}

void PepperView::SetView(const pp::View& view) {
  bool view_changed = false;

  pp::Rect pp_size = view.GetRect();
  webrtc::DesktopSize new_dips_size(pp_size.width(), pp_size.height());
  float new_dips_to_device_scale = view.GetDeviceScale();

  if (!dips_size_.equals(new_dips_size) ||
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
    if (!dips_size_.equals(source_size_)) {
      dips_to_view_scale_ = dips_to_device_scale_;
      view_size_.set(ceilf(dips_size_.width() * dips_to_view_scale_),
                     ceilf(dips_size_.height() * dips_to_view_scale_));
    }

    // Create a 2D rendering context at the chosen frame dimensions.
    pp::Size pp_size = pp::Size(view_size_.width(), view_size_.height());
    graphics2d_ = pp::Graphics2D(instance_, pp_size, false);

    // Specify the scale from our coordinates to DIPs.
    graphics2d_.SetScale(1.0f / dips_to_view_scale_);

    bool result = instance_->BindGraphics(graphics2d_);

    // There is no good way to handle this error currently.
    DCHECK(result) << "Couldn't bind the device context.";
  }

  // Ignore clip rectangle provided by the browser because it may not be
  // correct. See crbug.com/360240 . In case when the plugin is not visible
  // (e.g. another tab is selected) |clip_area_| is set to empty rectangle,
  // otherwise it's set to a rectangle that covers the whole plugin.
  //
  // TODO(sergeyu): Use view.GetClipRect() here after bug 360240 is fixed.
  webrtc::DesktopRect new_clip =
      view.IsVisible() ? webrtc::DesktopRect::MakeWH(
                             ceilf(pp_size.width() * dips_to_view_scale_),
                             ceilf(pp_size.height() * dips_to_view_scale_))
                       : webrtc::DesktopRect();
  if (!clip_area_.equals(new_clip)) {
    view_changed = true;

    // YUV to RGB conversion may require even X and Y coordinates for
    // the top left corner of the clipping area.
    clip_area_ = AlignRect(new_clip);
    clip_area_.IntersectWith(webrtc::DesktopRect::MakeSize(view_size_));
  }

  if (view_changed) {
    producer_->SetOutputSizeAndClip(view_size_, clip_area_);
    Initialize(producer_);
  }
}

void PepperView::ApplyBuffer(const webrtc::DesktopSize& view_size,
                             const webrtc::DesktopRect& clip_area,
                             webrtc::DesktopFrame* buffer,
                             const webrtc::DesktopRegion& region,
                             const webrtc::DesktopRegion& shape) {
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
  if (!view_size_.equals(view_size)) {
    FreeBuffer(buffer);
    Initialize(producer_);
  } else {
    FlushBuffer(clip_area, buffer, region);
    instance_->SetDesktopShape(shape);
  }
}

void PepperView::ReturnBuffer(webrtc::DesktopFrame* buffer) {
  DCHECK(context_->main_task_runner()->BelongsToCurrentThread());

  // Reuse the buffer if it is large enough, otherwise drop it on the floor
  // and allocate a new one.
  if (buffer->size().width() >= clip_area_.width() &&
      buffer->size().height() >= clip_area_.height()) {
    producer_->DrawBuffer(buffer);
  } else {
    FreeBuffer(buffer);
    Initialize(producer_);
  }
}

void PepperView::SetSourceSize(const webrtc::DesktopSize& source_size,
                               const webrtc::DesktopVector& source_dpi) {
  DCHECK(context_->main_task_runner()->BelongsToCurrentThread());

  if (source_size_.equals(source_size) && source_dpi_.equals(source_dpi))
    return;

  source_size_ = source_size;
  source_dpi_ = source_dpi;

  // Notify JavaScript of the change in source size.
  instance_->SetDesktopSize(source_size, source_dpi);
}

FrameConsumer::PixelFormat PepperView::GetPixelFormat() {
  return FORMAT_BGRA;
}

webrtc::DesktopFrame* PepperView::AllocateBuffer() {
  if (buffers_.size() >= kMaxPendingBuffersCount)
    return NULL;

  if (clip_area_.width()==0 || clip_area_.height()==0)
    return NULL;

  // Create an image buffer of the required size, but don't zero it.
  pp::ImageData buffer_data(instance_,
                    PP_IMAGEDATAFORMAT_BGRA_PREMUL,
                    pp::Size(clip_area_.width(),
                             clip_area_.height()),
                    false);
  if (buffer_data.is_null()) {
    LOG(WARNING) << "Not enough memory for frame buffers.";
    return NULL;
  }

  webrtc::DesktopFrame* buffer = new PepperDesktopFrame(buffer_data);
  buffers_.push_back(buffer);
  return buffer;
}

void PepperView::FreeBuffer(webrtc::DesktopFrame* buffer) {
  DCHECK(std::find(buffers_.begin(), buffers_.end(), buffer) != buffers_.end());

  buffers_.remove(buffer);
  delete buffer;
}

void PepperView::FlushBuffer(const webrtc::DesktopRect& clip_area,
                             webrtc::DesktopFrame* buffer,
                             const webrtc::DesktopRegion& region) {
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

  for (webrtc::DesktopRegion::Iterator i(region); !i.IsAtEnd(); i.Advance()) {
    webrtc::DesktopRect rect = i.rect();

    // Re-clip |region| with the current clipping area |clip_area_| because
    // the latter could change from the time the buffer was drawn.
    rect.IntersectWith(clip_area_);
    if (rect.is_empty())
      continue;

    // Specify the rectangle coordinates relative to the clipping area.
    rect.Translate(-clip_area.left(), -clip_area.top());

    // Pepper Graphics 2D has a strange and badly documented API that the
    // point here is the offset from the source rect. Why?
    graphics2d_.PaintImageData(
        static_cast<PepperDesktopFrame*>(buffer)->buffer(),
        pp::Point(clip_area.left(), clip_area.top()),
        pp::Rect(rect.left(), rect.top(), rect.width(), rect.height()));
  }

  // Notify the producer that some parts of the region weren't painted because
  // the clipping area has changed already.
  if (!clip_area.equals(clip_area_)) {
    webrtc::DesktopRegion not_painted = region;
    not_painted.Subtract(clip_area_);
    if (!not_painted.is_empty()) {
      producer_->InvalidateRegion(not_painted);
    }
  }

  // Flush the updated areas to the screen.
  pp::CompletionCallback callback =
      callback_factory_.NewCallback(&PepperView::OnFlushDone,
                                    start_time,
                                    buffer);
  int error = graphics2d_.Flush(callback);
  CHECK(error == PP_OK_COMPLETIONPENDING);
  flush_pending_ = true;
}

void PepperView::OnFlushDone(int result,
                             const base::Time& paint_start,
                             webrtc::DesktopFrame* buffer) {
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
