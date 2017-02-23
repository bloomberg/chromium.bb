// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/OffscreenCanvasFrameDispatcherImpl.h"

#include "cc/output/compositor_frame.h"
#include "cc/quads/texture_draw_quad.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/Histogram.h"
#include "platform/WebTaskRunner.h"
#include "platform/graphics/OffscreenCanvasPlaceholder.h"
#include "platform/graphics/gpu/SharedGpuContext.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "public/platform/modules/offscreencanvas/offscreen_canvas_surface.mojom-blink.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/transform.h"
#include "wtf/typed_arrays/ArrayBuffer.h"
#include "wtf/typed_arrays/Uint8Array.h"

namespace blink {

OffscreenCanvasFrameDispatcherImpl::OffscreenCanvasFrameDispatcherImpl(
    OffscreenCanvasFrameDispatcherClient* client,
    uint32_t clientId,
    uint32_t sinkId,
    int canvasId,
    int width,
    int height)
    : OffscreenCanvasFrameDispatcher(client),
      m_frameSinkId(cc::FrameSinkId(clientId, sinkId)),
      m_width(width),
      m_height(height),
      m_changeSizeForNextCommit(false),
      m_needsBeginFrame(false),
      m_nextResourceId(1u),
      m_binding(this),
      m_placeholderCanvasId(canvasId) {
  if (m_frameSinkId.is_valid()) {
    // Only frameless canvas pass an invalid frame sink id; we don't create
    // mojo channel for this special case.
    m_currentLocalSurfaceId = m_localSurfaceIdAllocator.GenerateId();
    DCHECK(!m_sink.is_bound());
    mojom::blink::OffscreenCanvasCompositorFrameSinkProviderPtr provider;
    Platform::current()->interfaceProvider()->getInterface(
        mojo::MakeRequest(&provider));
    provider->CreateCompositorFrameSink(m_frameSinkId,
                                        m_binding.CreateInterfacePtrAndBind(),
                                        mojo::MakeRequest(&m_sink));
  }
}

OffscreenCanvasFrameDispatcherImpl::~OffscreenCanvasFrameDispatcherImpl() {
}

void OffscreenCanvasFrameDispatcherImpl::setTransferableResourceToSharedBitmap(
    cc::TransferableResource& resource,
    RefPtr<StaticBitmapImage> image) {
  std::unique_ptr<cc::SharedBitmap> bitmap =
      Platform::current()->allocateSharedBitmap(IntSize(m_width, m_height));
  if (!bitmap)
    return;
  unsigned char* pixels = bitmap->pixels();
  DCHECK(pixels);
  SkImageInfo imageInfo = SkImageInfo::Make(
      m_width, m_height, kN32_SkColorType,
      image->isPremultiplied() ? kPremul_SkAlphaType : kUnpremul_SkAlphaType);
  // TODO(xlai): Optimize to avoid copying pixels. See crbug.com/651456.
  // However, in the case when |image| is texture backed, this function call
  // does a GPU readback which is required.
  // TODO(ccameron): Canvas should produce sRGB images.
  // https://crbug.com/672299
  image->imageForCurrentFrame(ColorBehavior::transformToGlobalTarget())
      ->readPixels(imageInfo, pixels, imageInfo.minRowBytes(), 0, 0);
  resource.mailbox_holder.mailbox = bitmap->id();
  resource.mailbox_holder.texture_target = 0;
  resource.is_software = true;

  // Hold ref to |bitmap|, to keep it alive until the browser ReclaimResources.
  // It guarantees that the shared bitmap is not re-used or deleted.
  m_sharedBitmaps.insert(m_nextResourceId, std::move(bitmap));
}

void OffscreenCanvasFrameDispatcherImpl::
    setTransferableResourceToSharedGPUContext(
        cc::TransferableResource& resource,
        RefPtr<StaticBitmapImage> image) {
  // TODO(crbug.com/652707): When committing the first frame, there is no
  // instance of SharedGpuContext yet, calling SharedGpuContext::gl() will
  // trigger a creation of an instace, which requires to create a
  // WebGraphicsContext3DProvider. This process is quite expensive, because
  // WebGraphicsContext3DProvider can only be constructed on the main thread,
  // and bind to the worker thread if commit() is called on worker. In the
  // subsequent frame, we should already have a SharedGpuContext, then getting
  // the gl interface should not be expensive.
  gpu::gles2::GLES2Interface* gl = SharedGpuContext::gl();

  SkImageInfo info = SkImageInfo::Make(
      m_width, m_height, kN32_SkColorType,
      image->isPremultiplied() ? kPremul_SkAlphaType : kUnpremul_SkAlphaType);
  RefPtr<ArrayBuffer> dstBuffer =
      ArrayBuffer::createOrNull(m_width * m_height, info.bytesPerPixel());
  // If it fails to create a buffer for copying the pixel data, then exit early.
  if (!dstBuffer)
    return;
  RefPtr<Uint8Array> dstPixels =
      Uint8Array::create(dstBuffer, 0, dstBuffer->byteLength());
  // TODO(ccameron): Canvas should produce sRGB images.
  // https://crbug.com/672299
  image->imageForCurrentFrame(ColorBehavior::transformToGlobalTarget())
      ->readPixels(info, dstPixels->data(), info.minRowBytes(), 0, 0);

  GLuint textureId = 0u;
  gl->GenTextures(1, &textureId);
  gl->BindTexture(GL_TEXTURE_2D, textureId);
  GLenum format =
      (kN32_SkColorType == kRGBA_8888_SkColorType) ? GL_RGBA : GL_BGRA_EXT;
  gl->TexImage2D(GL_TEXTURE_2D, 0, format, m_width, m_height, 0, format,
                 GL_UNSIGNED_BYTE, 0);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gl->TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height, format,
                    GL_UNSIGNED_BYTE, dstPixels->data());

  gpu::Mailbox mailbox;
  gl->GenMailboxCHROMIUM(mailbox.name);
  gl->ProduceTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name);

  const GLuint64 fenceSync = gl->InsertFenceSyncCHROMIUM();
  gl->ShallowFlushCHROMIUM();
  gpu::SyncToken syncToken;
  gl->GenSyncTokenCHROMIUM(fenceSync, syncToken.GetData());

  resource.mailbox_holder =
      gpu::MailboxHolder(mailbox, syncToken, GL_TEXTURE_2D);
  resource.read_lock_fences_enabled = false;
  resource.is_software = false;

  // Hold ref to |textureId| for the piece of GPU memory where the pixel data
  // is uploaded to, to keep it alive until the browser ReclaimResources.
  m_cachedTextureIds.insert(m_nextResourceId, textureId);
}

void OffscreenCanvasFrameDispatcherImpl::
    setTransferableResourceToStaticBitmapImage(
        cc::TransferableResource& resource,
        RefPtr<StaticBitmapImage> image) {
  image->ensureMailbox();
  resource.mailbox_holder =
      gpu::MailboxHolder(image->mailbox(), image->syncToken(), GL_TEXTURE_2D);
  resource.read_lock_fences_enabled = false;
  resource.is_software = false;

  // Hold ref to |image|, to keep it alive until the browser ReclaimResources.
  // It guarantees that the resource is not re-used or deleted.
  m_cachedImages.insert(m_nextResourceId, std::move(image));
}

namespace {

void updatePlaceholderImage(WeakPtr<OffscreenCanvasFrameDispatcher> dispatcher,
                            RefPtr<WebTaskRunner> taskRunner,
                            int placeholderCanvasId,
                            RefPtr<blink::StaticBitmapImage> image,
                            unsigned resourceId) {
  DCHECK(isMainThread());
  OffscreenCanvasPlaceholder* placeholderCanvas =
      OffscreenCanvasPlaceholder::getPlaceholderById(placeholderCanvasId);
  if (placeholderCanvas) {
    placeholderCanvas->setPlaceholderFrame(std::move(image),
                                           std::move(dispatcher),
                                           std::move(taskRunner), resourceId);
  }
}

}  // namespace

void OffscreenCanvasFrameDispatcherImpl::postImageToPlaceholder(
    RefPtr<StaticBitmapImage> image) {
  // After this point, |image| can only be used on the main thread, until
  // it is returned.
  image->transfer();
  RefPtr<WebTaskRunner> dispatcherTaskRunner =
      Platform::current()->currentThread()->getWebTaskRunner();

  Platform::current()->mainThread()->getWebTaskRunner()->postTask(
      BLINK_FROM_HERE,
      crossThreadBind(updatePlaceholderImage, this->createWeakPtr(),
                      WTF::passed(std::move(dispatcherTaskRunner)),
                      m_placeholderCanvasId, std::move(image),
                      m_nextResourceId));
  m_spareResourceLocks.insert(m_nextResourceId);
}

void OffscreenCanvasFrameDispatcherImpl::dispatchFrame(
    RefPtr<StaticBitmapImage> image,
    double commitStartTime,
    bool isWebGLSoftwareRendering /* This flag is true when WebGL's commit is
    called on SwiftShader. */) {
  if (!image || !verifyImageSize(image->size()))
    return;
  if (!m_frameSinkId.is_valid()) {
    postImageToPlaceholder(std::move(image));
    return;
  }
  cc::CompositorFrame frame;
  // TODO(crbug.com/652931): update the device_scale_factor
  frame.metadata.device_scale_factor = 1.0f;

  const gfx::Rect bounds(m_width, m_height);
  const int renderPassId = 1;
  std::unique_ptr<cc::RenderPass> pass = cc::RenderPass::Create();
  pass->SetNew(renderPassId, bounds, bounds, gfx::Transform());
  pass->has_transparent_background = false;

  cc::SharedQuadState* sqs = pass->CreateAndAppendSharedQuadState();
  sqs->SetAll(gfx::Transform(), bounds.size(), bounds, bounds, false, 1.f,
              SkBlendMode::kSrcOver, 0);

  cc::TransferableResource resource;
  resource.id = m_nextResourceId;
  resource.format = cc::ResourceFormat::RGBA_8888;
  resource.size = gfx::Size(m_width, m_height);
  // TODO(crbug.com/646022): making this overlay-able.
  resource.is_overlay_candidate = false;

  bool yflipped = false;
  OffscreenCanvasCommitType commitType;
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, commitTypeHistogram,
      new EnumerationHistogram("OffscreenCanvas.CommitType",
                               OffscreenCanvasCommitTypeCount));
  if (image->isTextureBacked()) {
    if (Platform::current()->isGPUCompositingEnabled() &&
        !isWebGLSoftwareRendering) {
      // Case 1: both canvas and compositor are gpu accelerated.
      commitType = CommitGPUCanvasGPUCompositing;
      setTransferableResourceToStaticBitmapImage(resource, image);
      yflipped = true;
    } else {
      // Case 2: canvas is accelerated but --disable-gpu-compositing is
      // specified, or WebGL's commit is called with SwiftShader. The latter
      // case is indicated by
      // WebGraphicsContext3DProvider::isSoftwareRendering.
      commitType = CommitGPUCanvasSoftwareCompositing;
      setTransferableResourceToSharedBitmap(resource, image);
    }
  } else {
    if (Platform::current()->isGPUCompositingEnabled() &&
        !isWebGLSoftwareRendering) {
      // Case 3: canvas is not gpu-accelerated, but compositor is
      commitType = CommitSoftwareCanvasGPUCompositing;
      setTransferableResourceToSharedGPUContext(resource, image);
    } else {
      // Case 4: both canvas and compositor are not gpu accelerated.
      commitType = CommitSoftwareCanvasSoftwareCompositing;
      setTransferableResourceToSharedBitmap(resource, image);
    }
  }

  postImageToPlaceholder(std::move(image));
  commitTypeHistogram.count(commitType);

  m_nextResourceId++;
  frame.resource_list.push_back(std::move(resource));

  cc::TextureDrawQuad* quad =
      pass->CreateAndAppendDrawQuad<cc::TextureDrawQuad>();
  gfx::Size rectSize(m_width, m_height);

  const bool needsBlending = true;
  // TOOD(crbug.com/645993): this should be inherited from WebGL context's
  // creation settings.
  const bool premultipliedAlpha = true;
  const gfx::PointF uvTopLeft(0.f, 0.f);
  const gfx::PointF uvBottomRight(1.f, 1.f);
  float vertexOpacity[4] = {1.f, 1.f, 1.f, 1.f};
  // TODO(crbug.com/645994): this should be true when using style
  // "image-rendering: pixelated".
  // TODO(crbug.com/645590): filter should respect the image-rendering CSS
  // property of associated canvas element.
  const bool nearestNeighbor = false;
  quad->SetAll(sqs, bounds, bounds, bounds, needsBlending, resource.id,
               gfx::Size(), premultipliedAlpha, uvTopLeft, uvBottomRight,
               SK_ColorTRANSPARENT, vertexOpacity, yflipped, nearestNeighbor,
               false);

  frame.render_pass_list.push_back(std::move(pass));

  double elapsedTime = WTF::monotonicallyIncreasingTime() - commitStartTime;

  switch (commitType) {
    case CommitGPUCanvasGPUCompositing:
      if (isMainThread()) {
        DEFINE_STATIC_LOCAL(
            CustomCountHistogram, commitGPUCanvasGPUCompositingMainTimer,
            ("Blink.Canvas.OffscreenCommit.GPUCanvasGPUCompositingMain", 0,
             10000000, 50));
        commitGPUCanvasGPUCompositingMainTimer.count(elapsedTime * 1000000.0);
      } else {
        DEFINE_THREAD_SAFE_STATIC_LOCAL(
            CustomCountHistogram, commitGPUCanvasGPUCompositingWorkerTimer,
            new CustomCountHistogram(
                "Blink.Canvas.OffscreenCommit.GPUCanvasGPUCompositingWorker", 0,
                10000000, 50));
        commitGPUCanvasGPUCompositingWorkerTimer.count(elapsedTime * 1000000.0);
      }
      break;
    case CommitGPUCanvasSoftwareCompositing:
      if (isMainThread()) {
        DEFINE_STATIC_LOCAL(
            CustomCountHistogram, commitGPUCanvasSoftwareCompositingMainTimer,
            ("Blink.Canvas.OffscreenCommit.GPUCanvasSoftwareCompositingMain", 0,
             10000000, 50));
        commitGPUCanvasSoftwareCompositingMainTimer.count(elapsedTime *
                                                          1000000.0);
      } else {
        DEFINE_THREAD_SAFE_STATIC_LOCAL(
            CustomCountHistogram, commitGPUCanvasSoftwareCompositingWorkerTimer,
            new CustomCountHistogram("Blink.Canvas.OffscreenCommit."
                                     "GPUCanvasSoftwareCompositingWorker",
                                     0, 10000000, 50));
        commitGPUCanvasSoftwareCompositingWorkerTimer.count(elapsedTime *
                                                            1000000.0);
      }
      break;
    case CommitSoftwareCanvasGPUCompositing:
      if (isMainThread()) {
        DEFINE_STATIC_LOCAL(
            CustomCountHistogram, commitSoftwareCanvasGPUCompositingMainTimer,
            ("Blink.Canvas.OffscreenCommit.SoftwareCanvasGPUCompositingMain", 0,
             10000000, 50));
        commitSoftwareCanvasGPUCompositingMainTimer.count(elapsedTime *
                                                          1000000.0);
      } else {
        DEFINE_THREAD_SAFE_STATIC_LOCAL(
            CustomCountHistogram, commitSoftwareCanvasGPUCompositingWorkerTimer,
            new CustomCountHistogram("Blink.Canvas.OffscreenCommit."
                                     "SoftwareCanvasGPUCompositingWorker",
                                     0, 10000000, 50));
        commitSoftwareCanvasGPUCompositingWorkerTimer.count(elapsedTime *
                                                            1000000.0);
      }
      break;
    case CommitSoftwareCanvasSoftwareCompositing:
      if (isMainThread()) {
        DEFINE_STATIC_LOCAL(CustomCountHistogram,
                            commitSoftwareCanvasSoftwareCompositingMainTimer,
                            ("Blink.Canvas.OffscreenCommit."
                             "SoftwareCanvasSoftwareCompositingMain",
                             0, 10000000, 50));
        commitSoftwareCanvasSoftwareCompositingMainTimer.count(elapsedTime *
                                                               1000000.0);
      } else {
        DEFINE_THREAD_SAFE_STATIC_LOCAL(
            CustomCountHistogram,
            commitSoftwareCanvasSoftwareCompositingWorkerTimer,
            new CustomCountHistogram("Blink.Canvas.OffscreenCommit."
                                     "SoftwareCanvasSoftwareCompositingWorker",
                                     0, 10000000, 50));
        commitSoftwareCanvasSoftwareCompositingWorkerTimer.count(elapsedTime *
                                                                 1000000.0);
      }
      break;
    case OffscreenCanvasCommitTypeCount:
      NOTREACHED();
  }

  if (m_changeSizeForNextCommit) {
    m_currentLocalSurfaceId = m_localSurfaceIdAllocator.GenerateId();
    m_changeSizeForNextCommit = false;
  }

  m_sink->SubmitCompositorFrame(m_currentLocalSurfaceId, std::move(frame));
}

void OffscreenCanvasFrameDispatcherImpl::DidReceiveCompositorFrameAck() {
  // TODO(fsamuel): Implement this.
}

void OffscreenCanvasFrameDispatcherImpl::setNeedsBeginFrame(
    bool needsBeginFrame) {
  if (m_sink && needsBeginFrame != m_needsBeginFrame) {
    m_needsBeginFrame = needsBeginFrame;
    m_sink->SetNeedsBeginFrame(needsBeginFrame);
  }
}

void OffscreenCanvasFrameDispatcherImpl::OnBeginFrame(
    const cc::BeginFrameArgs& beginFrameArgs) {
  DCHECK(client());
  client()->beginFrame();
}

void OffscreenCanvasFrameDispatcherImpl::ReclaimResources(
    const cc::ReturnedResourceArray& resources) {
  for (const auto& resource : resources) {
    RefPtr<StaticBitmapImage> image = m_cachedImages.at(resource.id);
    if (image)
      image->updateSyncToken(resource.sync_token);
    reclaimResource(resource.id);
  }
}

void OffscreenCanvasFrameDispatcherImpl::WillDrawSurface(
    const cc::LocalSurfaceId& localSurfaceId,
    ::gfx::mojom::blink::RectPtr damageRect) {
  // TODO(fsamuel, staraz): Implement this.
}

void OffscreenCanvasFrameDispatcherImpl::reclaimResource(unsigned resourceId) {
  // An image resource needs to be returned by both the
  // CompositorFrameSink and the HTMLCanvasElement. These
  // events can happen in any order.  The first of the two
  // to return a given resource will result in the spare
  // resource lock being lifted, and the second will delete
  // the resource for real.
  if (m_spareResourceLocks.contains(resourceId)) {
    m_spareResourceLocks.erase(resourceId);
    return;
  }
  m_cachedImages.erase(resourceId);
  m_sharedBitmaps.erase(resourceId);
  m_cachedTextureIds.erase(resourceId);
}

bool OffscreenCanvasFrameDispatcherImpl::verifyImageSize(
    const IntSize imageSize) {
  if (imageSize.width() == m_width && imageSize.height() == m_height)
    return true;
  return false;
}

void OffscreenCanvasFrameDispatcherImpl::reshape(int width, int height) {
  if (m_width != width || m_height != height) {
    m_width = width;
    m_height = height;
    m_changeSizeForNextCommit = true;
  }
}

}  // namespace blink
