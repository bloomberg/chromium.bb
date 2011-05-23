// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_view.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/size.h"
#include "remoting/base/tracer.h"
#include "remoting/base/util.h"
#include "remoting/client/chromoting_stats.h"
#include "remoting/client/client_context.h"
#include "remoting/client/plugin/chromoting_instance.h"
#include "remoting/client/plugin/pepper_util.h"

namespace remoting {

PepperView::PepperView(ChromotingInstance* instance, ClientContext* context)
  : instance_(instance),
    context_(context),
    screen_scale_(1.0),
    scale_to_fit_(false),
    host_width_(0),
    host_height_(0),
    client_width_(0),
    client_height_(0),
    is_static_fill_(false),
    static_fill_color_(0),
    ALLOW_THIS_IN_INITIALIZER_LIST(task_factory_(this)) {
}

PepperView::~PepperView() {
}

bool PepperView::Initialize() {
  return true;
}

void PepperView::TearDown() {
  DCHECK(CurrentlyOnPluginThread());

  task_factory_.RevokeAll();
}

void PepperView::Paint() {
  DCHECK(CurrentlyOnPluginThread());

  TraceContext::tracer()->PrintString("Start Paint.");
  // TODO(ajwong): We're assuming the native format is BGRA_PREMUL below. This
  // is wrong.
  if (is_static_fill_) {
    instance_->Log(logging::LOG_INFO,
                   "Static filling %08x", static_fill_color_);
    pp::ImageData image(instance_, pp::ImageData::GetNativeImageDataFormat(),
                        pp::Size(host_width_, host_height_),
                        false);
    if (image.is_null()) {
      instance_->Log(logging::LOG_ERROR,
                     "Unable to allocate image of size: %dx%d",
                     host_width_, host_height_);
      return;
    }

    for (int y = 0; y < image.size().height(); y++) {
      for (int x = 0; x < image.size().width(); x++) {
        *image.GetAddr32(pp::Point(x, y)) = static_fill_color_;
      }
    }

    // For ReplaceContents, make sure the image size matches the device context
    // size!  Otherwise, this will just silently do nothing.
    graphics2d_.ReplaceContents(&image);
    graphics2d_.Flush(TaskToCompletionCallback(
        task_factory_.NewRunnableMethod(&PepperView::OnPaintDone,
                                        base::Time::Now())));
  } else {
    // TODO(ajwong): We need to keep a backing store image of the host screen
    // that has the data here which can be redrawn.
    return;
  }
  TraceContext::tracer()->PrintString("End Paint.");
}

void PepperView::PaintFrame(media::VideoFrame* frame, UpdatedRects* rects) {
  DCHECK(CurrentlyOnPluginThread());

  TraceContext::tracer()->PrintString("Start Paint Frame.");

  SetViewport(0, 0, frame->width(), frame->height());

  uint8* frame_data = frame->data(media::VideoFrame::kRGBPlane);
  const int kFrameStride = frame->stride(media::VideoFrame::kRGBPlane);
  const int kBytesPerPixel = GetBytesPerPixel(media::VideoFrame::RGB32);

  if (!backing_store_.get() || backing_store_->is_null()) {
    instance_->Log(logging::LOG_ERROR, "Backing store is not available.");
    return;
  }
  if (DoScaling()) {
    if (!scaled_backing_store_.get() || scaled_backing_store_->is_null()) {
      instance_->Log(logging::LOG_ERROR,
                     "Scaled backing store is not available.");
    }
  }

  // Copy updated regions to the backing store and then paint the regions.
  for (size_t i = 0; i < rects->size(); ++i) {
    // TODO(ajwong): We're assuming the native format is BGRA_PREMUL below. This
    // is wrong.
    const gfx::Rect& r = (*rects)[i];

    // TODO(hclam): Make sure rectangles are valid.
    if (r.width() <= 0 || r.height() <= 0)
      continue;

    uint8* in = frame_data + kFrameStride * r.y() + kBytesPerPixel * r.x();
    uint8* out = reinterpret_cast<uint8*>(backing_store_->data()) +
        backing_store_->stride() * r.y() + kBytesPerPixel * r.x();

    // TODO(hclam): We really should eliminate this memory copy.
    for (int j = 0; j < r.height(); ++j) {
      memcpy(out, in, r.width() * kBytesPerPixel);
      in += kFrameStride;
      out += backing_store_->stride();
    }

    if (DoScaling()) {
      gfx::Rect r_scaled = UpdateScaledBackingStore(r);
      graphics2d_.PaintImageData(*scaled_backing_store_.get(), pp::Point(0, 0),
                                 pp::Rect(r_scaled.x(), r_scaled.y(),
                                          r_scaled.width(), r_scaled.height()));
    } else {
      // Pepper Graphics 2D has a strange and badly documented API that the
      // point here is the offset from the source rect. Why?
      graphics2d_.PaintImageData(*backing_store_.get(), pp::Point(0, 0),
                                 pp::Rect(r.x(), r.y(), r.width(), r.height()));
    }
  }

  graphics2d_.Flush(TaskToCompletionCallback(
      task_factory_.NewRunnableMethod(&PepperView::OnPaintDone,
                                      base::Time::Now())));

  TraceContext::tracer()->PrintString("End Paint Frame.");
}

gfx::Rect PepperView::UpdateScaledBackingStore(const gfx::Rect& r) {
  const int kBytesPerPixel = GetBytesPerPixel(media::VideoFrame::RGB32);
  // Find the updated rectangle in the scaled backing store.
  gfx::Point top_left = ConvertHostToScreen(r.origin());
  gfx::Point bottom_right = ConvertHostToScreen(gfx::Point(r.right(),
                                                           r.bottom()));
  int r_scaled_left = top_left.x();
  int r_scaled_right = bottom_right.x();
  int r_scaled_top = top_left.y();
  int r_scaled_bottom = bottom_right.y();
  if (r_scaled_right <= r_scaled_left || r_scaled_bottom <= r_scaled_top)
    return gfx::Rect(r_scaled_left, r_scaled_top, 0, 0);

  // Allow for the fact that ConvertHostToScreen and ConvertScreenToHost aren't
  // exact inverses.
  r_scaled_right++;
  r_scaled_bottom++;

  // Clip the scaled rectangle.
  r_scaled_left = std::max(r_scaled_left, 0);
  r_scaled_left = std::min(r_scaled_left, client_width_);
  r_scaled_right = std::max(r_scaled_right, 0);
  r_scaled_right = std::min(r_scaled_right, client_width_);
  r_scaled_top = std::max(r_scaled_top, 0);
  r_scaled_top = std::min(r_scaled_top, client_height_);
  r_scaled_bottom = std::max(r_scaled_bottom, 0);
  r_scaled_bottom = std::min(r_scaled_bottom, client_height_);

  // Blit from the backing store to the scaled backing store.
  for (int y_scaled = r_scaled_top; y_scaled < r_scaled_bottom; y_scaled++) {
    int y = ConvertScreenToHost(gfx::Point(0, y_scaled)).y();
    // Special case where each pixel is a word.
    if ((kBytesPerPixel == 4) && (backing_store_->stride() % 4 == 0) &&
        (scaled_backing_store_->stride() % 4 == 0)) {
      uint32* from = reinterpret_cast<uint32*>(backing_store_->data()) +
          (y * backing_store_->stride() / 4);
      uint32* to = reinterpret_cast<uint32*>(scaled_backing_store_->data()) +
          (y_scaled * scaled_backing_store_->stride() / 4) + r_scaled_left;
      uint32* to_max = to + (r_scaled_right - r_scaled_left);
      const int* offset = &screen_x_to_host_x_[r_scaled_left];
      while (to < to_max)
        *to++ = from[*offset++];
    } else {
      // Currently that special case is the only case that's ever encountered.
      NOTREACHED();
      uint8* from = reinterpret_cast<uint8*>(backing_store_->data()) +
          (y * backing_store_->stride());
      uint8* to = reinterpret_cast<uint8*>(scaled_backing_store_->data()) +
          (y_scaled * scaled_backing_store_->stride()) +
          (r_scaled_left * kBytesPerPixel);
      for (int x_sc = r_scaled_left; x_sc < r_scaled_right; x_sc++) {
        int x = screen_x_to_host_x_[x_sc];
        memcpy(to, from + (x * kBytesPerPixel), kBytesPerPixel);
        to += kBytesPerPixel;
      }
    }
  }
  return gfx::Rect(r_scaled_left, r_scaled_top, r_scaled_right - r_scaled_left,
                   r_scaled_bottom - r_scaled_top);
}

void PepperView::BlankRect(pp::ImageData& image_data, const gfx::Rect& rect) {
  const int kBytesPerPixel = GetBytesPerPixel(media::VideoFrame::RGB32);
  for (int y = rect.y(); y < rect.bottom(); y++) {
    uint8* to = reinterpret_cast<uint8*>(image_data.data()) +
        (y * image_data.stride()) + (rect.x() * kBytesPerPixel);
    memset(to, 0xff, rect.width() * kBytesPerPixel);
  }
}

void PepperView::SetSolidFill(uint32 color) {
  DCHECK(CurrentlyOnPluginThread());

  is_static_fill_ = true;
  static_fill_color_ = color;
}

void PepperView::UnsetSolidFill() {
  DCHECK(CurrentlyOnPluginThread());

  is_static_fill_ = false;
}

void PepperView::SetConnectionState(ConnectionState state) {
  DCHECK(CurrentlyOnPluginThread());

  // TODO(hclam): Re-consider the way we communicate with Javascript.
  ChromotingScriptableObject* scriptable_obj = instance_->GetScriptableObject();
  switch (state) {
    case CREATED:
      SetSolidFill(kCreatedColor);
      scriptable_obj->SetConnectionInfo(STATUS_CONNECTING, QUALITY_UNKNOWN);
      break;

    case CONNECTED:
      UnsetSolidFill();
      scriptable_obj->SignalLoginChallenge();
      break;

    case DISCONNECTED:
      SetSolidFill(kDisconnectedColor);
      scriptable_obj->SetConnectionInfo(STATUS_CLOSED, QUALITY_UNKNOWN);
      break;

    case FAILED:
      SetSolidFill(kFailedColor);
      scriptable_obj->SetConnectionInfo(STATUS_FAILED, QUALITY_UNKNOWN);
      break;
  }
}

void PepperView::UpdateLoginStatus(bool success, const std::string& info) {
  DCHECK(CurrentlyOnPluginThread());

  // TODO(hclam): Re-consider the way we communicate with Javascript.
  ChromotingScriptableObject* scriptable_obj = instance_->GetScriptableObject();
  if (success)
    scriptable_obj->SetConnectionInfo(STATUS_CONNECTED, QUALITY_UNKNOWN);
  else
    scriptable_obj->SignalLoginChallenge();
}

void PepperView::SetViewport(int x, int y, int width, int height) {
  DCHECK(CurrentlyOnPluginThread());

  if ((width == host_width_) && (height == host_height_))
    return;

  host_width_ = width;
  host_height_ = height;

  ResizeInternals();

  instance_->GetScriptableObject()->SetDesktopSize(host_width_, host_height_);
}

gfx::Point PepperView::ConvertScreenToHost(const gfx::Point& p) const {
  DCHECK(CurrentlyOnPluginThread());

  int x = static_cast<int>(p.x() / screen_scale_);
  x = std::min(x, host_width_ - 1);
  x = std::max(x, 0);
  int y = static_cast<int>(p.y() / screen_scale_);
  y = std::min(y, host_height_ - 1);
  y = std::max(y, 0);
  return gfx::Point(x, y);
}

gfx::Point PepperView::ConvertHostToScreen(const gfx::Point& p) const {
  return gfx::Point(static_cast<int>(p.x() * screen_scale_),
                    static_cast<int>(p.y() * screen_scale_));
}

void PepperView::SetScreenScale(double screen_scale) {
  if (screen_scale == screen_scale_)
    return;
  screen_scale_ = screen_scale;
  ResizeInternals();
  RefreshPaint();
}

void PepperView::RefreshPaint() {
  if (DoScaling()) {
    // Render from the whole backing store, to the scaled backing store, at the
    // current scale.
    gfx::Rect updated_rect = UpdateScaledBackingStore(gfx::Rect(
        host_width_, host_height_));
    // If the scale has just decreased, then there is stale raster data
    // to the right of, and below, the scaled copy of the host screen that's
    // just been rendered into the scaled backing store.
    // So blank out everything to the east of that copy...
    BlankRect(*scaled_backing_store_, gfx::Rect(
        updated_rect.right(), 0,
        scaled_backing_store_->size().width() - updated_rect.right(),
        updated_rect.bottom()));
    // ...and everything to the south and south-east.
    BlankRect(*scaled_backing_store_, gfx::Rect(
        0, updated_rect.bottom(), scaled_backing_store_->size().width(),
        scaled_backing_store_->size().height() - updated_rect.bottom()));
    graphics2d_.PaintImageData(*scaled_backing_store_.get(), pp::Point(0, 0),
                               pp::Rect(scaled_backing_store_->size()));
    graphics2d_.Flush(TaskToCompletionCallback(
        task_factory_.NewRunnableMethod(&PepperView::OnRefreshPaintDone)));
  } else {
    graphics2d_.PaintImageData(*backing_store_.get(), pp::Point(0, 0),
                               pp::Rect(backing_store_->size()));
    graphics2d_.Flush(TaskToCompletionCallback(
        task_factory_.NewRunnableMethod(&PepperView::OnRefreshPaintDone)));
  }
}

void PepperView::ResizeInternals() {
  graphics2d_ = pp::Graphics2D(instance_,
                               pp::Size(host_width_, host_height_),
                               false);
  if (!instance_->BindGraphics(graphics2d_)) {
    instance_->Log(logging::LOG_ERROR, "Couldn't bind the device context.");
    return;
  }

  if (host_width_ == 0 && host_height_ == 0)
    return;

  // Allocate the backing store to save the desktop image.
  pp::Size host_size(host_width_, host_height_);
  if ((backing_store_.get() == NULL) || (backing_store_->size() != host_size)) {
    backing_store_.reset(
        new pp::ImageData(instance_, pp::ImageData::GetNativeImageDataFormat(),
                          host_size, false));
    DCHECK(backing_store_.get() && !backing_store_->is_null())
        << "Not enough memory for backing store.";
  }

  // Allocate the scaled backing store.
  // This is the same size as |graphics2d_|, so that it can be used to blank out
  // stale regions of |graphics2d_|, as well as to update |graphics2d_| with
  // fresh data.
  if (DoScaling()) {
    if ((scaled_backing_store_.get() == NULL) ||
        (scaled_backing_store_->size() != host_size)) {
      scaled_backing_store_.reset(
          new pp::ImageData(instance_,
                            pp::ImageData::GetNativeImageDataFormat(),
                            host_size, false));
      DCHECK(scaled_backing_store_.get() && !scaled_backing_store_->is_null())
          << "Not enough memory for scaled backing store.";
    }
  } else {
    scaled_backing_store_.reset();
  }

  // Cache the horizontal component of the map from client screen co-ordinates
  // to host screen co-ordinates.
  screen_x_to_host_x_.reset(new int[client_width_]);
  for (int x = 0; x < client_width_; x++)
    screen_x_to_host_x_[x] = ConvertScreenToHost(gfx::Point(x, 0)).x();
}

bool PepperView::DoScaling() const {
  return (screen_scale_ != 1.0);
}

void PepperView::SetScreenSize(int width, int height) {
  DCHECK(CurrentlyOnPluginThread());

  client_width_ = width;
  client_height_ = height;
  if (!scale_to_fit_)
    return;
  if (host_width_ == 0 || host_height_ == 0) {
    SetScreenScale(1.0);
    return;
  }
  double scale_x = double(client_width_) / double(host_width_);
  double scale_y = double(client_height_) / double(host_height_);
  double scale = std::min(scale_x, scale_y);
  SetScreenScale(scale);
}

void PepperView::SetScaleToFit(bool scale_to_fit) {
  DCHECK(CurrentlyOnPluginThread());

  if (scale_to_fit == scale_to_fit_)
    return;
  scale_to_fit_ = scale_to_fit;
  if (scale_to_fit_) {
    SetScreenSize(client_width_, client_height_);
  } else {
    SetScreenScale(1.0);
  }
}

void PepperView::AllocateFrame(media::VideoFrame::Format format,
                               size_t width,
                               size_t height,
                               base::TimeDelta timestamp,
                               base::TimeDelta duration,
                               scoped_refptr<media::VideoFrame>* frame_out,
                               Task* done) {
  DCHECK(CurrentlyOnPluginThread());

  media::VideoFrame::CreateFrame(media::VideoFrame::RGB32,
                                 width, height,
                                 base::TimeDelta(), base::TimeDelta(),
                                 frame_out);
  if (*frame_out) {
    (*frame_out)->AddRef();
  }
  done->Run();
  delete done;
}

void PepperView::ReleaseFrame(media::VideoFrame* frame) {
  DCHECK(CurrentlyOnPluginThread());

  if (frame) {
    instance_->Log(logging::LOG_WARNING, "Frame released.");
    frame->Release();
  }
}

void PepperView::OnPartialFrameOutput(media::VideoFrame* frame,
                                      UpdatedRects* rects,
                                      Task* done) {
  DCHECK(CurrentlyOnPluginThread());

  TraceContext::tracer()->PrintString("Calling PaintFrame");
  // TODO(ajwong): Clean up this API to be async so we don't need to use a
  // member variable as a hack.
  PaintFrame(frame, rects);
  done->Run();
  delete done;
}

void PepperView::OnPaintDone(base::Time paint_start) {
  DCHECK(CurrentlyOnPluginThread());
  TraceContext::tracer()->PrintString("Paint flushed");
  instance_->GetStats()->video_paint_ms()->Record(
      (base::Time::Now() - paint_start).InMilliseconds());
  return;
}

void PepperView::OnRefreshPaintDone() {
  DCHECK(CurrentlyOnPluginThread());
}

}  // namespace remoting
