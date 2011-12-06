// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_view.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/size.h"
#include "remoting/base/util.h"
#include "remoting/client/chromoting_stats.h"
#include "remoting/client/client_context.h"
#include "remoting/client/plugin/chromoting_instance.h"
#include "remoting/client/plugin/pepper_util.h"

namespace remoting {

namespace {

ChromotingScriptableObject::ConnectionError ConvertConnectionError(
    protocol::ConnectionToHost::Error error) {
  switch (error) {
    case protocol::ConnectionToHost::OK:
      return ChromotingScriptableObject::ERROR_NONE;
    case protocol::ConnectionToHost::HOST_IS_OFFLINE:
      return ChromotingScriptableObject::ERROR_HOST_IS_OFFLINE;
    case protocol::ConnectionToHost::SESSION_REJECTED:
      return ChromotingScriptableObject::ERROR_SESSION_REJECTED;
    case protocol::ConnectionToHost::INCOMPATIBLE_PROTOCOL:
      return ChromotingScriptableObject::ERROR_INCOMPATIBLE_PROTOCOL;
    case protocol::ConnectionToHost::NETWORK_FAILURE:
      return ChromotingScriptableObject::ERROR_NETWORK_FAILURE;
  }
  DLOG(FATAL) << "Unknown error code" << error;
  return  ChromotingScriptableObject::ERROR_NONE;
}

}  // namespace

PepperView::PepperView(ChromotingInstance* instance, ClientContext* context)
  : instance_(instance),
    context_(context),
    flush_blocked_(false),
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
  DCHECK(context_->main_message_loop()->BelongsToCurrentThread());

  task_factory_.RevokeAll();
}

void PepperView::Paint() {
  DCHECK(context_->main_message_loop()->BelongsToCurrentThread());

  if (is_static_fill_) {
    VLOG(1) << "Static filling " << static_fill_color_;
    pp::ImageData image(instance_, pp::ImageData::GetNativeImageDataFormat(),
                        pp::Size(graphics2d_.size().width(),
                                 graphics2d_.size().height()),
                        false);
    if (image.is_null()) {
      LOG(ERROR) << "Unable to allocate image of size: "
                 << graphics2d_.size().width() << " x "
                 << graphics2d_.size().height();
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
    FlushGraphics(base::Time::Now());
  } else {
    // TODO(ajwong): We need to keep a backing store image of the host screen
    // that has the data here which can be redrawn.
    return;
  }
}

void PepperView::SetHostSize(const SkISize& host_size) {
  DCHECK(context_->main_message_loop()->BelongsToCurrentThread());

  if (host_size_ == host_size)
    return;

  host_size_ = host_size;

  // Submit an update of desktop size to Javascript.
  instance_->GetScriptableObject()->SetDesktopSize(
      host_size.width(), host_size.height());
}

void PepperView::PaintFrame(media::VideoFrame* frame, RectVector* rects) {
  DCHECK(context_->main_message_loop()->BelongsToCurrentThread());

  SetHostSize(SkISize::Make(frame->width(), frame->height()));

  if (!backing_store_.get() || backing_store_->is_null()) {
    LOG(ERROR) << "Backing store is not available.";
    return;
  }

  base::Time start_time = base::Time::Now();

  // Copy updated regions to the backing store and then paint the regions.
  bool changes_made = false;
  for (size_t i = 0; i < rects->size(); ++i)
    changes_made |= PaintRect(frame, (*rects)[i]);

  if (changes_made)
    FlushGraphics(start_time);
}

bool PepperView::PaintRect(media::VideoFrame* frame, const SkIRect& r) {
  const uint8* frame_data = frame->data(media::VideoFrame::kRGBPlane);
  const int kFrameStride = frame->stride(media::VideoFrame::kRGBPlane);
  const int kBytesPerPixel = GetBytesPerPixel(media::VideoFrame::RGB32);

  pp::Size backing_store_size = backing_store_->size();
  SkIRect rect(r);
  if (!rect.intersect(SkIRect::MakeWH(backing_store_size.width(),
                                      backing_store_size.height()))) {
    return false;
  }

  const uint8* in =
      frame_data +
      kFrameStride * rect.fTop +   // Y offset.
      kBytesPerPixel * rect.fLeft;  // X offset.
  uint8* out =
      reinterpret_cast<uint8*>(backing_store_->data()) +
      backing_store_->stride() * rect.fTop +  // Y offset.
      kBytesPerPixel * rect.fLeft;  // X offset.

  // TODO(hclam): We really should eliminate this memory copy.
  for (int j = 0; j < rect.height(); ++j) {
    memcpy(out, in, rect.width() * kBytesPerPixel);
    in += kFrameStride;
    out += backing_store_->stride();
  }

  // Pepper Graphics 2D has a strange and badly documented API that the
  // point here is the offset from the source rect. Why?
  graphics2d_.PaintImageData(
      *backing_store_.get(),
      pp::Point(0, 0),
      pp::Rect(rect.fLeft, rect.fTop, rect.width(), rect.height()));
  return true;
}

void PepperView::BlankRect(pp::ImageData& image_data, const pp::Rect& rect) {
  const int kBytesPerPixel = GetBytesPerPixel(media::VideoFrame::RGB32);
  for (int y = rect.y(); y < rect.bottom(); y++) {
    uint8* to = reinterpret_cast<uint8*>(image_data.data()) +
        (y * image_data.stride()) + (rect.x() * kBytesPerPixel);
    memset(to, 0xff, rect.width() * kBytesPerPixel);
  }
}

void PepperView::FlushGraphics(base::Time paint_start) {
  scoped_ptr<Task> task(
      task_factory_.NewRunnableMethod(&PepperView::OnPaintDone, paint_start));

  // Flag needs to be set here in order to get a proper error code for Flush().
  // Otherwise Flush() will always return PP_OK_COMPLETIONPENDING and the error
  // would be hidden.
  //
  // Note that we can also handle this by providing an actual callback which
  // takes the result code. Right now everything goes to the task that doesn't
  // result value.
  pp::CompletionCallback pp_callback(&CompletionCallbackTaskAdapter,
                                     task.get(),
                                     PP_COMPLETIONCALLBACK_FLAG_OPTIONAL);
  int error = graphics2d_.Flush(pp_callback);

  // There is already a flush in progress so set this flag to true so that we
  // can flush again later.
  // |paint_start| is then discarded but this is fine because we're not aiming
  // for precise measurement of timing, otherwise we need to keep a list of
  // queued start time(s).
  if (error == PP_ERROR_INPROGRESS)
    flush_blocked_ = true;
  else
    flush_blocked_ = false;

  // If Flush() returns asynchronously then release the task.
  if (error == PP_OK_COMPLETIONPENDING)
    ignore_result(task.release());
}

void PepperView::SetSolidFill(uint32 color) {
  DCHECK(context_->main_message_loop()->BelongsToCurrentThread());

  is_static_fill_ = true;
  static_fill_color_ = color;

  Paint();
}

void PepperView::UnsetSolidFill() {
  DCHECK(context_->main_message_loop()->BelongsToCurrentThread());

  is_static_fill_ = false;
}

void PepperView::SetConnectionState(protocol::ConnectionToHost::State state,
                                    protocol::ConnectionToHost::Error error) {
  DCHECK(context_->main_message_loop()->BelongsToCurrentThread());

  // TODO(hclam): Re-consider the way we communicate with Javascript.
  ChromotingScriptableObject* scriptable_obj = instance_->GetScriptableObject();
  switch (state) {
    case protocol::ConnectionToHost::CONNECTING:
      SetSolidFill(kCreatedColor);
      scriptable_obj->SetConnectionStatus(
          ChromotingScriptableObject::STATUS_CONNECTING,
          ConvertConnectionError(error));
      break;

    case protocol::ConnectionToHost::CONNECTED:
      UnsetSolidFill();
      scriptable_obj->SetConnectionStatus(
          ChromotingScriptableObject::STATUS_CONNECTED,
          ConvertConnectionError(error));
      break;

    case protocol::ConnectionToHost::CLOSED:
      SetSolidFill(kDisconnectedColor);
      scriptable_obj->SetConnectionStatus(
          ChromotingScriptableObject::STATUS_CLOSED,
          ConvertConnectionError(error));
      break;

    case protocol::ConnectionToHost::FAILED:
      SetSolidFill(kFailedColor);
      scriptable_obj->SetConnectionStatus(
          ChromotingScriptableObject::STATUS_FAILED,
          ConvertConnectionError(error));
      break;
  }
}

bool PepperView::SetPluginSize(const SkISize& plugin_size) {
  if (plugin_size_ == plugin_size)
    return false;
  plugin_size_ = plugin_size;

  pp::Size pp_size = pp::Size(plugin_size.width(), plugin_size.height());

  graphics2d_ = pp::Graphics2D(instance_, pp_size, true);
  if (!instance_->BindGraphics(graphics2d_)) {
    LOG(ERROR) << "Couldn't bind the device context.";
    return false;
  }

  if (plugin_size.isEmpty())
    return false;

  // Allocate the backing store to save the desktop image.
  if ((backing_store_.get() == NULL) ||
      (backing_store_->size() != pp_size)) {
    VLOG(1) << "Allocate backing store: "
            << plugin_size.width() << " x " << plugin_size.height();
    backing_store_.reset(
        new pp::ImageData(instance_, pp::ImageData::GetNativeImageDataFormat(),
                          pp_size, false));
    DCHECK(backing_store_.get() && !backing_store_->is_null())
        << "Not enough memory for backing store.";
  }
  return true;
}

double PepperView::GetHorizontalScaleRatio() const {
  if (instance_->DoScaling()) {
    DCHECK(!host_size_.isEmpty());
    return 1.0 * plugin_size_.width() / host_size_.width();
  }
  return 1.0;
}

double PepperView::GetVerticalScaleRatio() const {
  if (instance_->DoScaling()) {
    DCHECK(!host_size_.isEmpty());
    return 1.0 * plugin_size_.height() / host_size_.height();
  }
  return 1.0;
}

void PepperView::AllocateFrame(media::VideoFrame::Format format,
                               const SkISize& size,
                               scoped_refptr<media::VideoFrame>* frame_out,
                               const base::Closure& done) {
  DCHECK(context_->main_message_loop()->BelongsToCurrentThread());

  *frame_out = media::VideoFrame::CreateFrame(
      media::VideoFrame::RGB32, size.width(), size.height(),
      base::TimeDelta(), base::TimeDelta());
  (*frame_out)->AddRef();
  done.Run();
}

void PepperView::ReleaseFrame(media::VideoFrame* frame) {
  DCHECK(context_->main_message_loop()->BelongsToCurrentThread());

  if (frame)
    frame->Release();
}

void PepperView::OnPartialFrameOutput(media::VideoFrame* frame,
                                      RectVector* rects,
                                      const base::Closure& done) {
  DCHECK(context_->main_message_loop()->BelongsToCurrentThread());

  // TODO(ajwong): Clean up this API to be async so we don't need to use a
  // member variable as a hack.
  PaintFrame(frame, rects);
  done.Run();
}

void PepperView::OnPaintDone(base::Time paint_start) {
  DCHECK(context_->main_message_loop()->BelongsToCurrentThread());
  instance_->GetStats()->video_paint_ms()->Record(
      (base::Time::Now() - paint_start).InMilliseconds());

  // If the last flush failed because there was already another one in progress
  // then we perform the flush now.
  if (flush_blocked_)
    FlushGraphics(base::Time::Now());
  return;
}

}  // namespace remoting
