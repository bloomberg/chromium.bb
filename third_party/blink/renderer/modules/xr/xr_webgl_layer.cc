// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_webgl_layer.h"

#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/modules/webgl/webgl2_rendering_context.h"
#include "third_party/blink/renderer/modules/webgl/webgl_framebuffer.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context.h"
#include "third_party/blink/renderer/modules/xr/xr.h"
#include "third_party/blink/renderer/modules/xr/xr_frame_provider.h"
#include "third_party/blink/renderer/modules/xr/xr_presentation_context.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_view.h"
#include "third_party/blink/renderer/modules/xr/xr_viewport.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/geometry/double_size.h"
#include "third_party/blink/renderer/platform/geometry/float_point.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"

namespace blink {

namespace {

const double kFramebufferMinScale = 0.2;

const double kViewportMinScale = 0.2;
const double kViewportMaxScale = 1.0;

// Because including base::ClampToRange would be a dependency violation
double ClampToRange(const double value, const double min, const double max) {
  return std::min(std::max(value, min), max);
}

}  // namespace

XRWebGLLayer* XRWebGLLayer::Create(
    XRSession* session,
    const WebGLRenderingContextOrWebGL2RenderingContext& context,
    const XRWebGLLayerInit* initializer,
    ExceptionState& exception_state) {
  if (session->ended()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot create an XRWebGLLayer for an "
                                      "XRSession which has already ended.");
    return nullptr;
  }

  WebGLRenderingContextBase* webgl_context;
  if (context.IsWebGL2RenderingContext()) {
    webgl_context = context.GetAsWebGL2RenderingContext();
  } else {
    webgl_context = context.GetAsWebGLRenderingContext();
  }

  if (webgl_context->isContextLost()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot create an XRWebGLLayer with a "
                                      "lost WebGL context.");
    return nullptr;
  }

  if (!webgl_context->IsXRCompatible()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "This context is not marked as XR compatible.");
    return nullptr;
  }

  bool want_antialiasing = initializer->antialias();
  bool want_depth_buffer = initializer->depth();
  bool want_stencil_buffer = initializer->stencil();
  bool want_alpha_channel = initializer->alpha();
  bool want_ignore_depth_values = initializer->ignoreDepthValues();

  double framebuffer_scale = 1.0;

  if (initializer->hasFramebufferScaleFactor()) {
    // The max size will be either the native resolution or the default
    // if that happens to be larger than the native res. (That can happen on
    // desktop systems.)
    double max_scale = std::max(session->NativeFramebufferScale(), 1.0);

    // Clamp the developer-requested framebuffer size to ensure it's not too
    // small to see or unreasonably large.
    // TODO: Would be best to have the max value communicated from the service
    // rather than limited to the native res.
    framebuffer_scale = ClampToRange(initializer->framebufferScaleFactor(),
                                     kFramebufferMinScale, max_scale);
  }

  DoubleSize framebuffers_size = session->DefaultFramebufferSize();

  IntSize desired_size(framebuffers_size.Width() * framebuffer_scale,
                       framebuffers_size.Height() * framebuffer_scale);

  // Create an opaque WebGL Framebuffer
  WebGLFramebuffer* framebuffer = WebGLFramebuffer::CreateOpaque(webgl_context);

  scoped_refptr<XRWebGLDrawingBuffer> drawing_buffer =
      XRWebGLDrawingBuffer::Create(webgl_context->GetDrawingBuffer(),
                                   framebuffer->Object(), desired_size,
                                   want_alpha_channel, want_depth_buffer,
                                   want_stencil_buffer, want_antialiasing);

  if (!drawing_buffer) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Unable to create a framebuffer.");
    return nullptr;
  }

  // TODO: In the future this should be communicated by the drawing buffer and
  // indicate whether the depth buffers are being supplied to the XR compositor.
  bool compositor_supports_depth_values = false;

  // The ignoreDepthValues attribute of XRWebGLLayer may only be set to false if
  // the compositor is actually making use of the depth values and the user did
  // not set ignoreDepthValues to true explicitly.
  bool ignore_depth_values =
      !compositor_supports_depth_values || want_ignore_depth_values;

  return MakeGarbageCollected<XRWebGLLayer>(
      session, webgl_context, std::move(drawing_buffer), framebuffer,
      framebuffer_scale, ignore_depth_values);
}

XRWebGLLayer::XRWebGLLayer(XRSession* session,
                           WebGLRenderingContextBase* webgl_context,
                           scoped_refptr<XRWebGLDrawingBuffer> drawing_buffer,
                           WebGLFramebuffer* framebuffer,
                           double framebuffer_scale,
                           bool ignore_depth_values)
    : XRLayer(session, kXRWebGLLayerType),
      webgl_context_(webgl_context),
      drawing_buffer_(std::move(drawing_buffer)),
      framebuffer_(framebuffer),
      framebuffer_scale_(framebuffer_scale),
      ignore_depth_values_(ignore_depth_values) {
  DCHECK(drawing_buffer_);
  // If the contents need mirroring, indicate that to the drawing buffer.
  if (session->immersive() && session->External()) {
    can_mirror_ = true;

    mirror_client_ = base::AdoptRef(new XRWebGLDrawingBuffer::MirrorClient());
    drawing_buffer_->SetMirrorClient(mirror_client_);
  }
  UpdateViewports();
}

XRWebGLLayer::~XRWebGLLayer() {
  if (can_mirror_) {
    drawing_buffer_->SetMirrorClient(nullptr);
    mirror_client_->BeginDestruction();
  }
  mirror_client_ = nullptr;
  drawing_buffer_->BeginDestruction();
}

void XRWebGLLayer::getXRWebGLRenderingContext(
    WebGLRenderingContextOrWebGL2RenderingContext& result) const {
  if (webgl_context_->ContextType() == Platform::kWebGL2ContextType) {
    result.SetWebGL2RenderingContext(
        static_cast<WebGL2RenderingContext*>(webgl_context_.Get()));
  } else {
    result.SetWebGLRenderingContext(
        static_cast<WebGLRenderingContext*>(webgl_context_.Get()));
  }
}

XRViewport* XRWebGLLayer::getViewport(XRView* view) {
  if (!view || view->session() != session())
    return nullptr;

  return GetViewportForEye(view->EyeValue());
}

XRViewport* XRWebGLLayer::GetViewportForEye(XRView::XREye eye) {
  if (viewports_dirty_)
    UpdateViewports();

  if (eye == XRView::kEyeLeft)
    return left_viewport_;

  return right_viewport_;
}

void XRWebGLLayer::requestViewportScaling(double scale_factor) {
  if (!session()->immersive()) {
    // TODO(bajones): For the moment we're just going to ignore viewport changes
    // in non-immersive mode. This is legal, but probably not what developers
    // would like to see. Look into making viewport scale apply properly.
    scale_factor = 1.0;
  } else {
    // Clamp the developer-requested viewport size to ensure it's not too
    // small to see or larger than the framebuffer.
    scale_factor =
        ClampToRange(scale_factor, kViewportMinScale, kViewportMaxScale);
  }

  // Don't set this as the viewport_scale_ directly, since that would allow the
  // viewports to change mid-frame.
  requested_viewport_scale_ = scale_factor;
}

double XRWebGLLayer::getNativeFramebufferScaleFactor(XRSession* session) {
  return session->NativeFramebufferScale();
}

void XRWebGLLayer::UpdateViewports() {
  uint32_t framebuffer_width = framebufferWidth();
  uint32_t framebuffer_height = framebufferHeight();

  viewports_dirty_ = false;

  if (session()->immersive()) {
    if (session()->StereoscopicViews()) {
      left_viewport_ = MakeGarbageCollected<XRViewport>(
          0, 0, framebuffer_width * 0.5 * viewport_scale_,
          framebuffer_height * viewport_scale_);
      right_viewport_ = MakeGarbageCollected<XRViewport>(
          framebuffer_width * 0.5 * viewport_scale_, 0,
          framebuffer_width * 0.5 * viewport_scale_,
          framebuffer_height * viewport_scale_);
    } else {
      // Phone immersive AR only uses one viewport, but the second viewport is
      // needed for the UpdateLayerBounds mojo call which currently expects
      // exactly two views. This should be revisited as part of a refactor to
      // handle a more general list of viewports, cf. https://crbug.com/928433.
      left_viewport_ = MakeGarbageCollected<XRViewport>(
          0, 0, framebuffer_width * viewport_scale_,
          framebuffer_height * viewport_scale_);
      right_viewport_ = nullptr;
    }

    session()->xr()->frameProvider()->UpdateWebGLLayerViewports(this);

    // When mirroring make sure to also update the mirrored canvas UVs so it
    // only shows a single eye's data, cropped to display proportionally.
    if (session()->outputContext()) {
      float source_pixels_left = left_viewport_->x();
      float source_pixels_right = left_viewport_->x() + left_viewport_->width();
      float source_pixels_bottom = left_viewport_->y();
      float source_pixels_top = left_viewport_->y() + left_viewport_->height();

      // Adjust the UVs so that the mirrored content always fills the canvas
      // and is centered while staying proportional.
      DoubleSize output_size = session()->OutputCanvasSize();
      double output_aspect = output_size.Width() / output_size.Height();
      double viewport_aspect = static_cast<float>(left_viewport_->width()) /
                               static_cast<float>(left_viewport_->height());

      if (output_aspect > viewport_aspect) {
        // Output is wider than rendered image, scale to height and chop off top
        // and bottom.
        float cropped_image_height = left_viewport_->width() / output_aspect;
        float crop_amount = (left_viewport_->height() - cropped_image_height);
        source_pixels_top -= crop_amount / 2;
        source_pixels_bottom += crop_amount / 2;

      } else {
        // Output is taller relatively than rendered image, scale to width and
        // chop of left and right.
        float cropped_image_width = left_viewport_->height() * output_aspect;
        float crop_amount = (left_viewport_->width() - cropped_image_width);
        source_pixels_left += crop_amount / 2;
        source_pixels_right -= crop_amount / 2;
      }

      float uv_left = source_pixels_left / framebuffer_width;
      float uv_right = source_pixels_right / framebuffer_width;
      float uv_top = source_pixels_top / framebuffer_height;
      float uv_bottom = source_pixels_bottom / framebuffer_height;

      // Finally, in UV space (0, 0) is top-left corner, so we need to flip.
      uv_top = 1 - uv_top;
      uv_bottom = 1 - uv_bottom;
      session()->outputContext()->SetUV(FloatPoint(uv_left, uv_top),
                                        FloatPoint(uv_right, uv_bottom));
    }
  } else {
    left_viewport_ = MakeGarbageCollected<XRViewport>(
        0, 0, framebuffer_width * viewport_scale_,
        framebuffer_height * viewport_scale_);
  }
}

void XRWebGLLayer::OnFrameStart(
    const base::Optional<gpu::MailboxHolder>& buffer_mailbox_holder) {
  // If the requested scale has changed since the last from, update it now.
  if (viewport_scale_ != requested_viewport_scale_) {
    viewport_scale_ = requested_viewport_scale_;
    viewports_dirty_ = true;
  }

  framebuffer_->MarkOpaqueBufferComplete(true);
  framebuffer_->SetContentsChanged(false);
  if (buffer_mailbox_holder) {
    drawing_buffer_->UseSharedBuffer(buffer_mailbox_holder.value());
    is_direct_draw_frame = true;
  } else {
    is_direct_draw_frame = false;
  }
}

void XRWebGLLayer::OnFrameEnd() {
  framebuffer_->MarkOpaqueBufferComplete(false);
  if (is_direct_draw_frame) {
    drawing_buffer_->DoneWithSharedBuffer();
    is_direct_draw_frame = false;
  }

  // Submit the frame to the XR compositor.
  if (session()->immersive()) {
    // Always call submit, but notify if the contents were changed or not.
    session()->xr()->frameProvider()->SubmitWebGLLayer(
        this, framebuffer_->HaveContentsChanged());
  } else if (session()->outputContext()) {
    // Nothing to do if the framebuffer contents have not changed.
    if (framebuffer_->HaveContentsChanged()) {
      ImageBitmap* image_bitmap =
          ImageBitmap::Create(TransferToStaticBitmapImage(nullptr));
      session()->outputContext()->SetImage(image_bitmap);
    }
  }
}

void XRWebGLLayer::OnResize() {
  if (!session()->immersive()) {
    // For non-immersive sessions a resize indicates we should adjust the
    // drawing buffer size to match the canvas.
    DoubleSize framebuffers_size = session()->DefaultFramebufferSize();

    IntSize desired_size(framebuffers_size.Width() * framebuffer_scale_,
                         framebuffers_size.Height() * framebuffer_scale_);
    drawing_buffer_->Resize(desired_size);
  }

  // With both immersive and non-immersive session the viewports should be
  // recomputed when the output canvas resizes.
  viewports_dirty_ = true;
}

scoped_refptr<StaticBitmapImage> XRWebGLLayer::TransferToStaticBitmapImage(
    std::unique_ptr<viz::SingleReleaseCallback>* out_release_callback) {
  return drawing_buffer_->TransferToStaticBitmapImage(out_release_callback);
}

void XRWebGLLayer::UpdateWebXRMirror() {
  XRPresentationContext* mirror_context = session()->outputContext();
  if (can_mirror_ && mirror_context) {
    scoped_refptr<StaticBitmapImage> image = mirror_client_->GetLastImage();
    if (image) {
      ImageBitmap* image_bitmap = ImageBitmap::Create(std::move(image));
      mirror_context->SetImage(image_bitmap);
      mirror_client_->CallLastReleaseCallback();
    }
  }
}

void XRWebGLLayer::Trace(blink::Visitor* visitor) {
  visitor->Trace(left_viewport_);
  visitor->Trace(right_viewport_);
  visitor->Trace(webgl_context_);
  visitor->Trace(framebuffer_);
  XRLayer::Trace(visitor);
}

}  // namespace blink
