// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_video_renderer_2d.h"

#include <functional>

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/size.h"
#include "remoting/base/util.h"
#include "remoting/client/chromoting_stats.h"
#include "remoting/client/client_context.h"
#include "remoting/client/frame_consumer_proxy.h"
#include "remoting/client/frame_producer.h"
#include "remoting/client/software_video_renderer.h"
#include "remoting/proto/video.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

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
                 nullptr),
    buffer_(buffer) {}

}  // namespace

namespace remoting {

namespace {

// The maximum number of image buffers to be allocated at any point of time.
const size_t kMaxPendingBuffersCount = 2;

}  // namespace

PepperVideoRenderer2D::PepperVideoRenderer2D()
    : instance_(nullptr),
      event_handler_(nullptr),
      merge_buffer_(nullptr),
      dips_to_device_scale_(1.0f),
      dips_to_view_scale_(1.0f),
      flush_pending_(false),
      frame_received_(false),
      debug_dirty_region_(false),
      callback_factory_(this),
      weak_factory_(this) {
}

PepperVideoRenderer2D::~PepperVideoRenderer2D() {
  // The producer should now return any pending buffers. At this point, however,
  // ReturnBuffer() tasks scheduled by the producer will not be delivered,
  // so we free all the buffers once the producer's queue is empty.
  base::WaitableEvent done_event(true, false);
  software_video_renderer_->RequestReturnBuffers(
      base::Bind(&base::WaitableEvent::Signal, base::Unretained(&done_event)));
  done_event.Wait();

  merge_buffer_ = nullptr;
  while (!buffers_.empty()) {
    FreeBuffer(buffers_.front());
  }
}

bool PepperVideoRenderer2D::Initialize(pp::Instance* instance,
                                       const ClientContext& context,
                                       EventHandler* event_handler) {
  DCHECK(CalledOnValidThread());
  DCHECK(!instance_);
  DCHECK(!event_handler_);
  DCHECK(instance);
  DCHECK(event_handler);

  instance_ = instance;
  event_handler_ = event_handler;
  scoped_ptr<FrameConsumerProxy> frame_consumer_proxy =
      make_scoped_ptr(new FrameConsumerProxy(weak_factory_.GetWeakPtr()));
  software_video_renderer_.reset(new SoftwareVideoRenderer(
      context.main_task_runner(), context.decode_task_runner(),
      frame_consumer_proxy.Pass()));

  return true;
}

void PepperVideoRenderer2D::OnViewChanged(const pp::View& view) {
  DCHECK(CalledOnValidThread());

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
    software_video_renderer_->SetOutputSizeAndClip(view_size_, clip_area_);
    AllocateBuffers();
  }
}

void PepperVideoRenderer2D::EnableDebugDirtyRegion(bool enable) {
  debug_dirty_region_ = enable;
}

void PepperVideoRenderer2D::OnSessionConfig(
    const protocol::SessionConfig& config) {
  DCHECK(CalledOnValidThread());

  software_video_renderer_->OnSessionConfig(config);
  AllocateBuffers();
}

ChromotingStats* PepperVideoRenderer2D::GetStats() {
  DCHECK(CalledOnValidThread());

  return software_video_renderer_->GetStats();
}

protocol::VideoStub* PepperVideoRenderer2D::GetVideoStub() {
  DCHECK(CalledOnValidThread());

  return software_video_renderer_->GetVideoStub();
}

void PepperVideoRenderer2D::ApplyBuffer(const webrtc::DesktopSize& view_size,
                                        const webrtc::DesktopRect& clip_area,
                                        webrtc::DesktopFrame* buffer,
                                        const webrtc::DesktopRegion& region,
                                        const webrtc::DesktopRegion* shape) {
  DCHECK(CalledOnValidThread());

  if (!frame_received_) {
    event_handler_->OnVideoFirstFrameReceived();
    frame_received_ = true;
  }
  // We cannot use the data in the buffer if its dimensions don't match the
  // current view size.
  if (!view_size_.equals(view_size)) {
    FreeBuffer(buffer);
    AllocateBuffers();
  } else {
    FlushBuffer(clip_area, buffer, region);
    if (shape) {
      if (!source_shape_ || !source_shape_->Equals(*shape)) {
        source_shape_ = make_scoped_ptr(new webrtc::DesktopRegion(*shape));
        event_handler_->OnVideoShape(source_shape_.get());
      }
    } else if (source_shape_) {
      source_shape_ = nullptr;
      event_handler_->OnVideoShape(nullptr);
    }
  }
}

void PepperVideoRenderer2D::ReturnBuffer(webrtc::DesktopFrame* buffer) {
  DCHECK(CalledOnValidThread());

  // Reuse the buffer if it is large enough, otherwise drop it on the floor
  // and allocate a new one.
  if (buffer->size().width() >= clip_area_.width() &&
      buffer->size().height() >= clip_area_.height()) {
    software_video_renderer_->DrawBuffer(buffer);
  } else {
    FreeBuffer(buffer);
    AllocateBuffers();
  }
}

void PepperVideoRenderer2D::SetSourceSize(
    const webrtc::DesktopSize& source_size,
    const webrtc::DesktopVector& source_dpi) {
  DCHECK(CalledOnValidThread());

  if (source_size_.equals(source_size) && source_dpi_.equals(source_dpi))
    return;

  source_size_ = source_size;
  source_dpi_ = source_dpi;

  // Notify JavaScript of the change in source size.
  event_handler_->OnVideoSize(source_size, source_dpi);
}

FrameConsumer::PixelFormat PepperVideoRenderer2D::GetPixelFormat() {
  return FORMAT_BGRA;
}

void PepperVideoRenderer2D::AllocateBuffers() {
  if (clip_area_.width() == 0 || clip_area_.height() == 0)
    return;

  while (buffers_.size() < kMaxPendingBuffersCount) {
    // Create an image buffer of the required size, but don't zero it.
    pp::ImageData buffer_data(instance_, PP_IMAGEDATAFORMAT_BGRA_PREMUL,
                              pp::Size(clip_area_.width(), clip_area_.height()),
                              false);
    if (buffer_data.is_null()) {
      LOG(WARNING) << "Not enough memory for frame buffers.";
      break;
    }

    webrtc::DesktopFrame* buffer = new PepperDesktopFrame(buffer_data);
    buffers_.push_back(buffer);
    software_video_renderer_->DrawBuffer(buffer);
  }
}

void PepperVideoRenderer2D::FreeBuffer(webrtc::DesktopFrame* buffer) {
  DCHECK(std::find(buffers_.begin(), buffers_.end(), buffer) != buffers_.end());

  buffers_.remove(buffer);
  delete buffer;
}

void PepperVideoRenderer2D::FlushBuffer(const webrtc::DesktopRect& clip_area,
                                        webrtc::DesktopFrame* buffer,
                                        const webrtc::DesktopRegion& region) {
  // Defer drawing if the flush is already in progress.
  if (flush_pending_) {
    // |merge_buffer_| is guaranteed to be free here because we allocate only
    // two buffers simultaneously. If more buffers are allowed this code should
    // apply all pending changes to the screen.
    DCHECK(merge_buffer_ == nullptr);

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
      software_video_renderer_->InvalidateRegion(not_painted);
    }
  }

  // Flush the updated areas to the screen.
  pp::CompletionCallback callback = callback_factory_.NewCallback(
      &PepperVideoRenderer2D::OnFlushDone, start_time, buffer);
  int error = graphics2d_.Flush(callback);
  CHECK(error == PP_OK_COMPLETIONPENDING);
  flush_pending_ = true;

  // If Debug dirty region is enabled then emit it.
  if (debug_dirty_region_) {
    event_handler_->OnVideoFrameDirtyRegion(region);
  }
}

void PepperVideoRenderer2D::OnFlushDone(int result,
                                        const base::Time& paint_start,
                                        webrtc::DesktopFrame* buffer) {
  DCHECK(CalledOnValidThread());
  DCHECK(flush_pending_);

  software_video_renderer_->GetStats()->video_paint_ms()->Record(
      (base::Time::Now() - paint_start).InMilliseconds());

  flush_pending_ = false;
  ReturnBuffer(buffer);

  // If there is a buffer queued for rendering then render it now.
  if (merge_buffer_) {
    buffer = merge_buffer_;
    merge_buffer_ = nullptr;
    FlushBuffer(merge_clip_area_, buffer, merge_region_);
  }
}

}  // namespace remoting
