// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"

#include <limits>
#include <utility>

#include "base/callback_helpers.h"
#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/checked_math.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/limits.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_metadata.h"
#include "media/base/video_frame_pool.h"
#include "media/base/video_types.h"
#include "media/base/video_util.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_plane_layout.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_cssimagevalue_htmlcanvaselement_htmlimageelement_htmlvideoelement_imagebitmap_offscreencanvas_svgimageelement_videoframe.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_color_space_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame_buffer_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame_copy_to_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_pixel_format.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_image_source.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/canvas/imagebitmap/image_bitmap_factories.h"
#include "third_party/blink/renderer/modules/webcodecs/dom_rect_util.h"
#include "third_party/blink/renderer/modules/webcodecs/parsed_copy_to_options.h"
#include "third_party/blink/renderer/modules/webcodecs/video_color_space.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/video_frame_image_util.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

media::VideoPixelFormat ToMediaPixelFormat(V8VideoPixelFormat::Enum fmt) {
  switch (fmt) {
    case V8VideoPixelFormat::Enum::kI420:
      return media::PIXEL_FORMAT_I420;
    case V8VideoPixelFormat::Enum::kI420A:
      return media::PIXEL_FORMAT_I420A;
    case V8VideoPixelFormat::Enum::kI422:
      return media::PIXEL_FORMAT_I422;
    case V8VideoPixelFormat::Enum::kI444:
      return media::PIXEL_FORMAT_I444;
    case V8VideoPixelFormat::Enum::kNV12:
      return media::PIXEL_FORMAT_NV12;
    case V8VideoPixelFormat::Enum::kRGBA:
      return media::PIXEL_FORMAT_ABGR;
    case V8VideoPixelFormat::Enum::kRGBX:
      return media::PIXEL_FORMAT_XBGR;
    case V8VideoPixelFormat::Enum::kBGRA:
      return media::PIXEL_FORMAT_ARGB;
    case V8VideoPixelFormat::Enum::kBGRX:
      return media::PIXEL_FORMAT_XRGB;
  }
}

media::VideoPixelFormat ToOpaqueMediaPixelFormat(media::VideoPixelFormat fmt) {
  DCHECK(!media::IsOpaque(fmt));
  switch (fmt) {
    case media::PIXEL_FORMAT_I420A:
      return media::PIXEL_FORMAT_I420;
    case media::PIXEL_FORMAT_ARGB:
      return media::PIXEL_FORMAT_XRGB;
    case media::PIXEL_FORMAT_ABGR:
      return media::PIXEL_FORMAT_XBGR;
    default:
      NOTIMPLEMENTED() << "Missing support for making " << fmt << " opaque.";
      return fmt;
  }
}

class CachedVideoFramePool : public GarbageCollected<CachedVideoFramePool>,
                             public Supplement<ExecutionContext> {
 public:
  static const char kSupplementName[];

  static CachedVideoFramePool& From(ExecutionContext& context) {
    CachedVideoFramePool* supplement =
        Supplement<ExecutionContext>::From<CachedVideoFramePool>(context);
    if (!supplement) {
      supplement = MakeGarbageCollected<CachedVideoFramePool>(context);
      Supplement<ExecutionContext>::ProvideTo(context, supplement);
    }
    return *supplement;
  }

  explicit CachedVideoFramePool(ExecutionContext& context)
      : Supplement<ExecutionContext>(context),
        task_runner_(Thread::Current()->GetTaskRunner()) {}
  virtual ~CachedVideoFramePool() = default;

  // Disallow copy and assign.
  CachedVideoFramePool& operator=(const CachedVideoFramePool&) = delete;
  CachedVideoFramePool(const CachedVideoFramePool&) = delete;

  scoped_refptr<media::VideoFrame> CreateFrame(media::VideoPixelFormat format,
                                               const gfx::Size& coded_size,
                                               const gfx::Rect& visible_rect,
                                               const gfx::Size& natural_size,
                                               base::TimeDelta timestamp) {
    if (!frame_pool_)
      CreatePoolAndStartIdleObsever();

    last_frame_creation_ = base::TimeTicks::Now();
    return frame_pool_->CreateFrame(format, coded_size, visible_rect,
                                    natural_size, timestamp);
  }

  void Trace(Visitor* visitor) const override {
    Supplement<ExecutionContext>::Trace(visitor);
  }

 private:
  static const base::TimeDelta kIdleTimeout;

  void PostMonitoringTask() {
    DCHECK(!task_handle_.IsActive());
    task_handle_ = PostDelayedCancellableTask(
        *task_runner_, FROM_HERE,
        WTF::Bind(&CachedVideoFramePool::PurgeIdleFramePool,
                  WrapWeakPersistent(this)),
        kIdleTimeout);
  }

  void CreatePoolAndStartIdleObsever() {
    DCHECK(!frame_pool_);
    frame_pool_ = std::make_unique<media::VideoFramePool>();
    PostMonitoringTask();
  }

  // We don't want a VideoFramePool to stick around forever wasting memory, so
  // once we haven't issued any VideoFrames for a while, turn down the pool.
  void PurgeIdleFramePool() {
    if (base::TimeTicks::Now() - last_frame_creation_ > kIdleTimeout) {
      frame_pool_.reset();
      return;
    }
    PostMonitoringTask();
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<media::VideoFramePool> frame_pool_;
  base::TimeTicks last_frame_creation_;
  TaskHandle task_handle_;
};

// static -- defined out of line to satisfy link time requirements.
const char CachedVideoFramePool::kSupplementName[] = "CachedVideoFramePool";
const base::TimeDelta CachedVideoFramePool::kIdleTimeout =
    base::TimeDelta::FromSeconds(10);

bool IsSupportedPlanarFormat(const media::VideoFrame& frame) {
  if (!frame.IsMappable() && !frame.HasGpuMemoryBuffer())
    return false;

  const size_t num_planes = frame.layout().num_planes();
  switch (frame.format()) {
    case media::PIXEL_FORMAT_I420:
    case media::PIXEL_FORMAT_I422:
    case media::PIXEL_FORMAT_I444:
      return num_planes == 3;
    case media::PIXEL_FORMAT_I420A:
      return num_planes == 4;
    case media::PIXEL_FORMAT_NV12:
      return num_planes == 2;
    case media::PIXEL_FORMAT_XBGR:
    case media::PIXEL_FORMAT_XRGB:
    case media::PIXEL_FORMAT_ABGR:
    case media::PIXEL_FORMAT_ARGB:
      return num_planes == 1;
    default:
      return false;
  }
}

}  // namespace

VideoFrame::VideoFrame(scoped_refptr<media::VideoFrame> frame,
                       ExecutionContext* context,
                       std::string monitoring_source_id) {
  DCHECK(frame);
  handle_ = base::MakeRefCounted<VideoFrameHandle>(
      frame, context, std::move(monitoring_source_id));

  external_allocated_memory_ =
      media::VideoFrame::AllocationSize(frame->format(), frame->coded_size());
  v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
      external_allocated_memory_);
}

VideoFrame::VideoFrame(scoped_refptr<VideoFrameHandle> handle)
    : handle_(std::move(handle)) {
  DCHECK(handle_);

  // The provided |handle| may be invalid if close() was called while
  // it was being sent to another thread.
  auto local_frame = handle_->frame();
  if (!local_frame)
    return;

  external_allocated_memory_ = media::VideoFrame::AllocationSize(
      local_frame->format(), local_frame->coded_size());
  v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
      external_allocated_memory_);
}

VideoFrame::~VideoFrame() {
  v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(
      -external_allocated_memory_);
}

// static
VideoFrame* VideoFrame::Create(ScriptState* script_state,
                               const V8CanvasImageSource* source,
                               const VideoFrameInit* init,
                               ExceptionState& exception_state) {
  auto* image_source = ToCanvasImageSource(source, exception_state);
  if (!image_source) {
    // ToCanvasImageSource() will throw a source appropriate exception.
    return nullptr;
  }

  if (image_source->WouldTaintOrigin()) {
    exception_state.ThrowSecurityError(
        "VideoFrames can't be created from tainted sources.");
    return nullptr;
  }

  constexpr char kAlphaDiscard[] = "discard";
  constexpr char kAlphaKeep[] = "keep";

  // Special case <video> and VideoFrame to directly use the underlying frame.
  if (source->IsVideoFrame() || source->IsHTMLVideoElement()) {
    scoped_refptr<media::VideoFrame> source_frame;
    switch (source->GetContentType()) {
      case V8CanvasImageSource::ContentType::kVideoFrame:
        source_frame = source->GetAsVideoFrame()->frame();
        if (!init->hasTimestamp() && !init->hasDuration() &&
            (init->alpha() == kAlphaKeep ||
             media::IsOpaque(source_frame->format()))) {
          return source->GetAsVideoFrame()->clone(exception_state);
        }
        break;
      case V8CanvasImageSource::ContentType::kHTMLVideoElement:
        if (auto* wmp = source->GetAsHTMLVideoElement()->GetWebMediaPlayer())
          source_frame = wmp->GetCurrentFrame();
        break;
      default:
        NOTREACHED();
    }

    if (!source_frame) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "Invalid source state");
      return nullptr;
    }

    const bool force_opaque = init->alpha() == kAlphaDiscard &&
                              !media::IsOpaque(source_frame->format());

    // We can't modify the timestamp or duration directly since there may be
    // other owners accessing these fields concurrently.
    if (init->hasTimestamp() || init->hasDuration() || force_opaque) {
      const auto wrapped_format =
          force_opaque ? ToOpaqueMediaPixelFormat(source_frame->format())
                       : source_frame->format();
      auto wrapped_frame = media::VideoFrame::WrapVideoFrame(
          source_frame, wrapped_format, source_frame->visible_rect(),
          source_frame->natural_size());
      wrapped_frame->set_color_space(source_frame->ColorSpace());
      if (init->hasTimestamp()) {
        wrapped_frame->set_timestamp(
            base::TimeDelta::FromMicroseconds(init->timestamp()));
      }
      if (init->hasDuration()) {
        wrapped_frame->metadata().frame_duration =
            base::TimeDelta::FromMicroseconds(init->duration());
      }
      source_frame = std::move(wrapped_frame);
    }

    return MakeGarbageCollected<VideoFrame>(
        std::move(source_frame), ExecutionContext::From(script_state));
  }

  // Some elements like OffscreenCanvas won't choose a default size, so we must
  // ask them what size they think they are first.
  auto source_size =
      image_source->ElementSize(FloatSize(), kRespectImageOrientation);

  SourceImageStatus status = kInvalidSourceImageStatus;
  auto image = image_source->GetSourceImageForCanvas(&status, source_size);
  if (!image || status != kNormalSourceImageStatus) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid source state");
    return nullptr;
  }

  const auto timestamp = base::TimeDelta::FromMicroseconds(
      (init && init->hasTimestamp()) ? init->timestamp() : 0);

  const auto paint_image = image->PaintImageForCurrentFrame();
  const auto sk_image_info = paint_image.GetSkImageInfo();
  auto sk_color_space = sk_image_info.refColorSpace();
  if (!sk_color_space)
    sk_color_space = SkColorSpace::MakeSRGB();

  auto gfx_color_space = gfx::ColorSpace(*sk_color_space);
  if (!gfx_color_space.IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid color space");
    return nullptr;
  }

  const gfx::Size coded_size(sk_image_info.width(), sk_image_info.height());
  const gfx::Rect visible_rect(coded_size);
  const gfx::Size natural_size = coded_size;
  const auto orientation = image->CurrentFrameOrientation().Orientation();

  sk_sp<SkImage> sk_image;
  scoped_refptr<media::VideoFrame> frame;
  if (image->IsTextureBacked() && SharedGpuContext::IsGpuCompositingEnabled()) {
    DCHECK(image->IsStaticBitmapImage());
    auto format = media::VideoPixelFormatFromSkColorType(
        paint_image.GetColorType(),
        image->CurrentFrameKnownToBeOpaque() || init->alpha() == kAlphaDiscard);

    auto* sbi = static_cast<StaticBitmapImage*>(image.get());
    gpu::MailboxHolder mailbox_holders[media::VideoFrame::kMaxPlanes] = {
        sbi->GetMailboxHolder()};
    const bool is_origin_top_left = sbi->IsOriginTopLeft();

    // The sync token needs to be updated when |frame| is released, but
    // AcceleratedStaticBitmapImage::UpdateSyncToken() is not thread-safe.
    auto release_cb = media::BindToCurrentLoop(WTF::Bind(
        [](scoped_refptr<Image> image, const gpu::SyncToken& sync_token) {
          static_cast<StaticBitmapImage*>(image.get())
              ->UpdateSyncToken(sync_token);
        },
        std::move(image)));

    frame = media::VideoFrame::WrapNativeTextures(
        format, mailbox_holders, std::move(release_cb), coded_size,
        visible_rect, natural_size, timestamp);

    if (frame)
      frame->metadata().texture_origin_is_top_left = is_origin_top_left;

    // Note: We could add the StaticBitmapImage to the VideoFrameHandle so we
    // can round trip through VideoFrame back to canvas w/o any copies, but
    // this doesn't seem like a common use case.
  } else {
    // Note: The current PaintImage may be lazy generated, for simplicity, we
    // just ask Skia to rasterize the image for us.
    //
    // A potential optimization could use PaintImage::DecodeYuv() to decode
    // directly into a media::VideoFrame. This would improve VideoFrame from
    // <img> creation, but probably such users should be using ImageDecoder
    // directly.
    sk_image = paint_image.GetSwSkImage();
    if (sk_image->isLazyGenerated())
      sk_image = sk_image->makeRasterImage();

    const bool force_opaque =
        init && init->alpha() == kAlphaDiscard && !sk_image->isOpaque();

    frame = media::CreateFromSkImage(sk_image, visible_rect, natural_size,
                                     timestamp, force_opaque);
  }

  if (!frame) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Failed to create video frame");
    return nullptr;
  }

  frame->set_color_space(gfx_color_space);
  if (init->hasDuration()) {
    frame->metadata().frame_duration =
        base::TimeDelta::FromMicroseconds(init->duration());
  }
  if (orientation != ImageOrientationEnum::kDefault) {
    frame->metadata().transformation =
        ImageOrientationToVideoTransformation(orientation);
  }
  return MakeGarbageCollected<VideoFrame>(
      base::MakeRefCounted<VideoFrameHandle>(
          std::move(frame), std::move(sk_image),
          ExecutionContext::From(script_state)));
}

VideoFrame* VideoFrame::Create(ScriptState* script_state,
                               const AllowSharedBufferSource* data,
                               const VideoFrameBufferInit* init,
                               ExceptionState& exception_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);

  // Handle format; the string was validated by the V8 binding.
  auto typed_fmt = V8VideoPixelFormat::Create(init->format());
  auto media_fmt = ToMediaPixelFormat(typed_fmt->AsEnum());

  // Validate coded size.
  uint32_t coded_width = init->codedWidth();
  uint32_t coded_height = init->codedHeight();
  if (coded_width == 0) {
    exception_state.ThrowTypeError("codedWidth must be nonzero.");
    return nullptr;
  }
  if (coded_height == 0) {
    exception_state.ThrowTypeError("codedHeight must be nonzero.");
    return nullptr;
  }
  if (coded_width > media::limits::kMaxDimension ||
      coded_height > media::limits::kMaxDimension ||
      coded_width * coded_height > media::limits::kMaxCanvas) {
    exception_state.ThrowTypeError(
        String::Format("Coded size %u x %u exceeds implementation limit.",
                       coded_width, coded_height));
    return nullptr;
  }
  const gfx::Size coded_size(static_cast<int>(coded_width),
                             static_cast<int>(coded_height));

  // Validate visibleRect and layout.
  VideoFrameCopyToOptions* adapted_init =
      MakeGarbageCollected<VideoFrameCopyToOptions>();
  if (init->hasVisibleRect())
    adapted_init->setRect(init->visibleRect());
  if (init->hasLayout())
    adapted_init->setLayout(init->layout());

  ParsedCopyToOptions copy_options(adapted_init, media_fmt, coded_size,
                                   gfx::Rect(coded_size), exception_state);
  if (exception_state.HadException())
    return nullptr;
  const gfx::Rect visible_rect = copy_options.rect;

  // Validate data.
  auto buffer = AsSpan<const uint8_t>(data);
  if (!buffer.data()) {
    exception_state.ThrowTypeError("data is detached.");
    return nullptr;
  }
  if (buffer.size() < copy_options.min_buffer_size) {
    exception_state.ThrowTypeError("data is not large enough.");
    return nullptr;
  }

  // Validate natural size.
  uint32_t natural_width = static_cast<uint32_t>(visible_rect.width());
  uint32_t natural_height = static_cast<uint32_t>(visible_rect.height());
  if (init->hasDisplayWidth() || init->hasDisplayHeight()) {
    if (!init->hasDisplayWidth()) {
      exception_state.ThrowTypeError(
          "displayHeight specified without displayWidth.");
      return nullptr;
    }
    if (!init->hasDisplayHeight()) {
      exception_state.ThrowTypeError(
          "displayWidth specified without displayHeight.");
      return nullptr;
    }

    natural_width = init->displayWidth();
    natural_height = init->displayHeight();
    if (natural_width == 0) {
      exception_state.ThrowTypeError("displayWidth must be nonzero.");
      return nullptr;
    }
    if (natural_height == 0) {
      exception_state.ThrowTypeError("displayHeight must be nonzero.");
      return nullptr;
    }
    // There is no limit on display size in //media; 2 * kMaxDimension is
    // arbitrary. A big difference is that //media computes display size such
    // that at least one dimension is the same as the visible size (and the
    // other is not smaller than the visible size), while WebCodecs apps can
    // specify any combination.
    //
    // Note that at large display sizes, it can become impossible to allocate
    // a texture large enough to render into. It may be impossible, for example,
    // to create an ImageBitmap without also scaling down.
    if (natural_width > 2 * media::limits::kMaxDimension ||
        natural_height > 2 * media::limits::kMaxDimension) {
      exception_state.ThrowTypeError(
          String::Format("Invalid display size (%u, %u); exceeds "
                         "implementation limit.",
                         natural_width, natural_height));
      return nullptr;
    }
  }
  const gfx::Size natural_size(static_cast<int>(natural_width),
                               static_cast<int>(natural_height));

  // Create a frame.
  const auto timestamp = base::TimeDelta::FromMicroseconds(init->timestamp());
  auto& frame_pool = CachedVideoFramePool::From(*execution_context);
  auto frame = frame_pool.CreateFrame(media_fmt, coded_size, visible_rect,
                                      natural_size, timestamp);
  if (!frame) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kOperationError,
        String::Format("Failed to create a VideoFrame with format: %s, "
                       "coded size: %s, visibleRect: %s, display size: %s.",
                       VideoPixelFormatToString(media_fmt).c_str(),
                       coded_size.ToString().c_str(),
                       visible_rect.ToString().c_str(),
                       natural_size.ToString().c_str()));
    return nullptr;
  }

  if (init->hasColorSpace()) {
    VideoColorSpace* video_color_space =
        MakeGarbageCollected<VideoColorSpace>(init->colorSpace());
    frame->set_color_space(video_color_space->ToGfxColorSpace());
  } else {
    // So far all WebCodecs YUV formats are planar, so this test works. That
    // might not be the case in the future.
    frame->set_color_space(media::IsYuvPlanar(media_fmt)
                               ? gfx::ColorSpace::CreateREC709()
                               : gfx::ColorSpace::CreateSRGB());
  }

  if (init->hasDuration()) {
    frame->metadata().frame_duration =
        base::TimeDelta::FromMicroseconds(init->duration());
  }

  // Copy planes.
  for (wtf_size_t i = 0; i < copy_options.num_planes; ++i) {
    libyuv::CopyPlane(buffer.data() + copy_options.planes[i].offset,
                      static_cast<int>(copy_options.planes[i].stride),
                      frame->data(i), static_cast<int>(frame->stride(i)),
                      static_cast<int>(copy_options.planes[i].width_bytes),
                      static_cast<int>(copy_options.planes[i].height));
  }

  return MakeGarbageCollected<VideoFrame>(std::move(frame),
                                          ExecutionContext::From(script_state));
}

absl::optional<V8VideoPixelFormat> VideoFrame::format() const {
  auto local_frame = handle_->frame();
  if (!local_frame || !IsSupportedPlanarFormat(*local_frame))
    return absl::nullopt;

  switch (local_frame->format()) {
    case media::PIXEL_FORMAT_I420:
      return V8VideoPixelFormat(V8VideoPixelFormat::Enum::kI420);
    case media::PIXEL_FORMAT_I420A:
      return V8VideoPixelFormat(V8VideoPixelFormat::Enum::kI420A);
    case media::PIXEL_FORMAT_I422:
      return V8VideoPixelFormat(V8VideoPixelFormat::Enum::kI422);
    case media::PIXEL_FORMAT_I444:
      return V8VideoPixelFormat(V8VideoPixelFormat::Enum::kI444);
    case media::PIXEL_FORMAT_NV12:
      return V8VideoPixelFormat(V8VideoPixelFormat::Enum::kNV12);
    case media::PIXEL_FORMAT_ABGR:
      return V8VideoPixelFormat(V8VideoPixelFormat::Enum::kRGBA);
    case media::PIXEL_FORMAT_XBGR:
      return V8VideoPixelFormat(V8VideoPixelFormat::Enum::kRGBX);
    case media::PIXEL_FORMAT_ARGB:
      return V8VideoPixelFormat(V8VideoPixelFormat::Enum::kBGRA);
    case media::PIXEL_FORMAT_XRGB:
      return V8VideoPixelFormat(V8VideoPixelFormat::Enum::kBGRX);
    default:
      NOTREACHED();
      return absl::nullopt;
  }
}

uint32_t VideoFrame::codedWidth() const {
  auto local_frame = handle_->frame();
  if (!local_frame)
    return 0;
  return local_frame->coded_size().width();
}

uint32_t VideoFrame::codedHeight() const {
  auto local_frame = handle_->frame();
  if (!local_frame)
    return 0;
  return local_frame->coded_size().height();
}

absl::optional<DOMRectReadOnly*> VideoFrame::codedRect() {
  auto local_frame = handle_->frame();
  if (!local_frame)
    return absl::nullopt;

  if (!coded_rect_) {
    coded_rect_ = MakeGarbageCollected<DOMRectReadOnly>(
        0, 0, local_frame->coded_size().width(),
        local_frame->coded_size().height());
  }
  return coded_rect_;
}

absl::optional<DOMRectReadOnly*> VideoFrame::visibleRect() {
  auto local_frame = handle_->frame();
  if (!local_frame)
    return absl::nullopt;

  if (!visible_rect_) {
    visible_rect_ = MakeGarbageCollected<DOMRectReadOnly>(
        local_frame->visible_rect().x(), local_frame->visible_rect().y(),
        local_frame->visible_rect().width(),
        local_frame->visible_rect().height());
  }
  return visible_rect_;
}

uint32_t VideoFrame::displayWidth() const {
  auto local_frame = handle_->frame();
  if (!local_frame)
    return 0;

  const auto transform =
      local_frame->metadata().transformation.value_or(media::kNoTransformation);
  if (transform == media::kNoTransformation ||
      transform.rotation == media::VIDEO_ROTATION_0 ||
      transform.rotation == media::VIDEO_ROTATION_180) {
    return local_frame->natural_size().width();
  }
  return local_frame->natural_size().height();
}

uint32_t VideoFrame::displayHeight() const {
  auto local_frame = handle_->frame();
  if (!local_frame)
    return 0;
  const auto transform =
      local_frame->metadata().transformation.value_or(media::kNoTransformation);
  if (transform == media::kNoTransformation ||
      transform.rotation == media::VIDEO_ROTATION_0 ||
      transform.rotation == media::VIDEO_ROTATION_180) {
    return local_frame->natural_size().height();
  }
  return local_frame->natural_size().width();
}

absl::optional<int64_t> VideoFrame::timestamp() const {
  auto local_frame = handle_->frame();
  if (!local_frame || local_frame->timestamp() == media::kNoTimestamp)
    return absl::nullopt;
  return local_frame->timestamp().InMicroseconds();
}

absl::optional<uint64_t> VideoFrame::duration() const {
  auto local_frame = handle_->frame();
  // TODO(sandersd): Can a duration be kNoTimestamp?
  if (!local_frame || !local_frame->metadata().frame_duration.has_value())
    return absl::nullopt;
  return local_frame->metadata().frame_duration->InMicroseconds();
}

VideoColorSpace* VideoFrame::colorSpace() {
  auto local_frame = handle_->frame();
  if (!local_frame) {
    if (!empty_color_space_)
      empty_color_space_ = MakeGarbageCollected<VideoColorSpace>();

    return empty_color_space_;
  }

  if (!color_space_) {
    color_space_ =
        MakeGarbageCollected<VideoColorSpace>(local_frame->ColorSpace());
  }
  return color_space_;
}

uint32_t VideoFrame::allocationSize(VideoFrameCopyToOptions* options,
                                    ExceptionState& exception_state) {
  auto local_frame = handle_->frame();
  if (!local_frame) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "VideoFrame is closed.");
    return 0;
  }

  // TODO(crbug.com/1176464): Determine the format readback will occur in, use
  // that to compute the layout.
  if (!IsSupportedPlanarFormat(*local_frame)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "allocationSize() is not yet implemented when format is null.");
    return 0;
  }

  ParsedCopyToOptions layout(options, local_frame->format(),
                             local_frame->coded_size(),
                             local_frame->visible_rect(), exception_state);
  if (exception_state.HadException())
    return 0;

  return layout.min_buffer_size;
}

ScriptPromise VideoFrame::copyTo(ScriptState* script_state,
                                 const AllowSharedBufferSource* destination,
                                 VideoFrameCopyToOptions* options,
                                 ExceptionState& exception_state) {
  auto local_frame = handle_->frame();
  if (!local_frame) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot copy closed VideoFrame.");
    return ScriptPromise();
  }

  // TODO(crbug.com/1176464): Use async texture readback.
  if (!IsSupportedPlanarFormat(*local_frame)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "copyTo() is not yet implemented when format is null.");
    return ScriptPromise();
  }

  // Compute layout.
  ParsedCopyToOptions layout(options, local_frame->format(),
                             local_frame->coded_size(),
                             local_frame->visible_rect(), exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  // Validate destination buffer.
  auto buffer = AsSpan<uint8_t>(destination);
  if (!buffer.data()) {
    exception_state.ThrowTypeError("destination is detached.");
    return ScriptPromise();
  }
  if (buffer.size() < layout.min_buffer_size) {
    exception_state.ThrowTypeError("destination is not large enough.");
    return ScriptPromise();
  }

  // Map buffers if necessary.
  if (!local_frame->IsMappable()) {
    DCHECK(local_frame->HasGpuMemoryBuffer());
    local_frame = media::ConvertToMemoryMappedFrame(local_frame);
    if (!local_frame) {
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "Failed to read VideoFrame data.");
      return ScriptPromise();
    }
  }

  // Copy data.
  for (wtf_size_t i = 0; i < layout.num_planes; i++) {
    uint8_t* src = local_frame->data(i) +
                   layout.planes[i].top * local_frame->stride(i) +
                   layout.planes[i].left_bytes;
    libyuv::CopyPlane(src, static_cast<int>(local_frame->stride(i)),
                      buffer.data() + layout.planes[i].offset,
                      static_cast<int>(layout.planes[i].stride),
                      static_cast<int>(layout.planes[i].width_bytes),
                      static_cast<int>(layout.planes[i].height));
  }

  // Convert and return |layout|.
  HeapVector<Member<PlaneLayout>> result;
  for (wtf_size_t i = 0; i < layout.num_planes; i++) {
    auto* plane = MakeGarbageCollected<PlaneLayout>();
    plane->setOffset(layout.planes[i].offset);
    plane->setStride(layout.planes[i].stride);
    result.push_back(plane);
  }
  return ScriptPromise::Cast(script_state, ToV8(result, script_state));
}

void VideoFrame::close() {
  handle_->Invalidate();
}

VideoFrame* VideoFrame::clone(ExceptionState& exception_state) {
  auto handle = handle_->Clone();
  if (!handle) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot clone closed VideoFrame.");
    return nullptr;
  }

  return MakeGarbageCollected<VideoFrame>(std::move(handle));
}

scoped_refptr<Image> VideoFrame::GetSourceImageForCanvas(
    SourceImageStatus* status,
    const FloatSize&,
    const AlphaDisposition alpha_disposition) {
  // UnpremultiplyAlpha is not implemented yet.
  DCHECK_EQ(alpha_disposition, kPremultiplyAlpha);

  const auto local_handle = handle_->CloneForInternalUse();
  if (!local_handle) {
    DLOG(ERROR) << "GetSourceImageForCanvas() called for closed frame.";
    *status = kInvalidSourceImageStatus;
    return nullptr;
  }

  const auto orientation_enum = VideoTransformationToImageOrientation(
      local_handle->frame()->metadata().transformation.value_or(
          media::kNoTransformation));
  if (auto sk_img = local_handle->sk_image()) {
    *status = kNormalSourceImageStatus;
    return UnacceleratedStaticBitmapImage::Create(std::move(sk_img),
                                                  orientation_enum);
  }

  const auto image = CreateImageFromVideoFrame(local_handle->frame());
  if (!image) {
    *status = kInvalidSourceImageStatus;
    return nullptr;
  }

  *status = kNormalSourceImageStatus;
  return image;
}

bool VideoFrame::WouldTaintOrigin() const {
  // VideoFrames can't be created from untainted sources currently. If we ever
  // add that ability we will need a tainting signal on the VideoFrame itself.
  // One example would be allowing <video> elements to provide a VideoFrame.
  return false;
}

FloatSize VideoFrame::ElementSize(
    const FloatSize& default_object_size,
    const RespectImageOrientationEnum respect_orientation) const {
  // BitmapSourceSize() will always ignore orientation.
  if (respect_orientation == kRespectImageOrientation) {
    auto local_frame = handle_->frame();
    if (!local_frame)
      return FloatSize();

    const auto orientation_enum = VideoTransformationToImageOrientation(
        local_frame->metadata().transformation.value_or(
            media::kNoTransformation));
    auto orientation_adjusted_size =
        FloatSize(local_frame->visible_rect().size());
    if (ImageOrientation(orientation_enum).UsesWidthAsHeight())
      return orientation_adjusted_size.TransposedSize();
    return orientation_adjusted_size;
  }
  return FloatSize(BitmapSourceSize());
}

bool VideoFrame::IsVideoFrame() const {
  return true;
}

bool VideoFrame::IsOpaque() const {
  if (auto local_frame = handle_->frame())
    return media::IsOpaque(local_frame->format());
  return false;
}

bool VideoFrame::IsAccelerated() const {
  if (auto local_handle = handle_->CloneForInternalUse()) {
    return handle_->sk_image() ? false
                               : WillCreateAcceleratedImagesFromVideoFrame(
                                     local_handle->frame().get());
  }
  return false;
}

IntSize VideoFrame::BitmapSourceSize() const {
  auto local_frame = handle_->frame();
  if (!local_frame)
    return IntSize();

  // ImageBitmaps should always return the size w/o respecting orientation.
  return IntSize(local_frame->visible_rect().size());
}

ScriptPromise VideoFrame::CreateImageBitmap(ScriptState* script_state,
                                            absl::optional<IntRect> crop_rect,
                                            const ImageBitmapOptions* options,
                                            ExceptionState& exception_state) {
  const auto local_handle = handle_->CloneForInternalUse();
  if (!local_handle) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot create ImageBitmap from closed VideoFrame.");
    return ScriptPromise();
  }

  const auto orientation_enum = VideoTransformationToImageOrientation(
      local_handle->frame()->metadata().transformation.value_or(
          media::kNoTransformation));
  if (auto sk_img = local_handle->sk_image()) {
    auto* image_bitmap = MakeGarbageCollected<ImageBitmap>(
        UnacceleratedStaticBitmapImage::Create(std::move(sk_img),
                                               orientation_enum),
        crop_rect, options);
    return ImageBitmapSource::FulfillImageBitmap(script_state, image_bitmap,
                                                 exception_state);
  }

  const auto image = CreateImageFromVideoFrame(local_handle->frame());
  if (!image) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        String(("Unsupported VideoFrame: " +
                local_handle->frame()->AsHumanReadableString())
                   .c_str()));
    return ScriptPromise();
  }

  auto* image_bitmap =
      MakeGarbageCollected<ImageBitmap>(image, crop_rect, options);
  return ImageBitmapSource::FulfillImageBitmap(script_state, image_bitmap,
                                               exception_state);
}

void VideoFrame::Trace(Visitor* visitor) const {
  visitor->Trace(coded_rect_);
  visitor->Trace(visible_rect_);
  visitor->Trace(color_space_);
  visitor->Trace(empty_color_space_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
