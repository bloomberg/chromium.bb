// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_raw_draw.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/process_memory_dump.h"
#include "cc/paint/paint_op_buffer.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "ui/gl/trace_util.h"

namespace gpu {
namespace {

size_t EstimatedSize(viz::ResourceFormat format, const gfx::Size& size) {
  size_t estimated_size = 0;
  viz::ResourceSizes::MaybeSizeInBytes(size, format, &estimated_size);
  return estimated_size;
}

}  // namespace

class SharedImageBackingRawDraw::RepresentationRaster
    : public SharedImageRepresentationRaster {
 public:
  RepresentationRaster(SharedImageManager* manager,
                       SharedImageBacking* backing,
                       MemoryTypeTracker* tracker)
      : SharedImageRepresentationRaster(manager, backing, tracker) {}
  ~RepresentationRaster() override = default;

  cc::PaintOpBuffer* BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      const absl::optional<SkColor>& clear_color) override {
    return raw_draw_backing()->BeginRasterWriteAccess(
        final_msaa_count, surface_props, clear_color);
  }

  void EndWriteAccess(base::OnceClosure callback) override {
    raw_draw_backing()->EndRasterWriteAccess(std::move(callback));
  }

  cc::PaintOpBuffer* BeginReadAccess(
      absl::optional<SkColor>& clear_color) override {
    return raw_draw_backing()->BeginRasterReadAccess(clear_color);
  }

  void EndReadAccess() override { raw_draw_backing()->EndReadAccess(); }

 private:
  SharedImageBackingRawDraw* raw_draw_backing() {
    return static_cast<SharedImageBackingRawDraw*>(backing());
  }
};

class SharedImageBackingRawDraw::RepresentationSkia
    : public SharedImageRepresentationSkia {
 public:
  RepresentationSkia(SharedImageManager* manager,
                     SharedImageBacking* backing,
                     MemoryTypeTracker* tracker)
      : SharedImageRepresentationSkia(manager, backing, tracker) {}

  bool SupportsMultipleConcurrentReadAccess() override { return true; }

  sk_sp<SkPromiseImageTexture> BeginWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override {
    NOTIMPLEMENTED();
    return nullptr;
  }

  void EndWriteAccess(sk_sp<SkSurface> surface) override { NOTIMPLEMENTED(); }

  sk_sp<SkPromiseImageTexture> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override {
    return raw_draw_backing()->BeginSkiaReadAccess();
  }

  void EndReadAccess() override { raw_draw_backing()->EndReadAccess(); }

 private:
  SharedImageBackingRawDraw* raw_draw_backing() {
    return static_cast<SharedImageBackingRawDraw*>(backing());
  }
};

SharedImageBackingRawDraw::SharedImageBackingRawDraw(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage)
    : ClearTrackingSharedImageBacking(mailbox,
                                      format,
                                      size,
                                      color_space,
                                      surface_origin,
                                      alpha_type,
                                      usage,
                                      /*estimated_size=*/0,
                                      /*is_thread_safe=*/true) {}

SharedImageBackingRawDraw::~SharedImageBackingRawDraw() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  AutoLock auto_lock(this);
  DCHECK_EQ(read_count_, 0);
  DCHECK(!is_write_);
  ResetPaintOpBuffer();
  DestroyBackendTexture();
}

bool SharedImageBackingRawDraw::ProduceLegacyMailbox(
    MailboxManager* mailbox_manager) {
  NOTIMPLEMENTED();
  return false;
}

void SharedImageBackingRawDraw::Update(
    std::unique_ptr<gfx::GpuFence> in_fence) {
  NOTIMPLEMENTED();
}

void SharedImageBackingRawDraw::OnMemoryDump(
    const std::string& dump_name,
    base::trace_event::MemoryAllocatorDump* dump,
    base::trace_event::ProcessMemoryDump* pmd,
    uint64_t client_tracing_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  AutoLock auto_lock(this);
  if (auto tracing_id = GrBackendTextureTracingID(backend_texture_)) {
    // Add a |service_guid| which expresses shared ownership between the
    // various GPU dumps.
    auto client_guid = GetSharedImageGUIDForTracing(mailbox());
    auto service_guid = gl::GetGLTextureServiceGUIDForTracing(tracing_id);
    pmd->CreateSharedGlobalAllocatorDump(service_guid);

    std::string format_dump_name =
        base::StringPrintf("%s/format=%d", dump_name.c_str(), format());
    base::trace_event::MemoryAllocatorDump* format_dump =
        pmd->CreateAllocatorDump(format_dump_name);
    format_dump->AddScalar(
        base::trace_event::MemoryAllocatorDump::kNameSize,
        base::trace_event::MemoryAllocatorDump::kUnitsBytes,
        static_cast<uint64_t>(EstimatedSize(format(), size())));

    int importance = 2;  // This client always owns the ref.
    pmd->AddOwnershipEdge(client_guid, service_guid, importance);
  }
}

std::unique_ptr<SharedImageRepresentationRaster>
SharedImageBackingRawDraw::ProduceRaster(SharedImageManager* manager,
                                         MemoryTypeTracker* tracker) {
  return std::make_unique<RepresentationRaster>(manager, this, tracker);
}

std::unique_ptr<SharedImageRepresentationSkia>
SharedImageBackingRawDraw::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!context_state_)
    context_state_ = context_state;
  DCHECK(context_state_ == context_state);
  return std::make_unique<RepresentationSkia>(manager, this, tracker);
}

void SharedImageBackingRawDraw::ResetPaintOpBuffer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!paint_op_buffer_) {
    DCHECK(!clear_color_);
    DCHECK(!paint_op_release_callback_);
    DCHECK(!backend_texture_.isValid());
    DCHECK(!promise_texture_);
    return;
  }

  clear_color_.reset();
  paint_op_buffer_->Reset();

  if (paint_op_release_callback_)
    std::move(paint_op_release_callback_).Run();
}

bool SharedImageBackingRawDraw::CreateBackendTextureAndFlushPaintOps() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!backend_texture_.isValid());
  DCHECK(!promise_texture_);

  auto mipmap = usage() & SHARED_IMAGE_USAGE_MIPMAP ? GrMipMapped::kYes
                                                    : GrMipMapped::kNo;
  auto sk_color = viz::ResourceFormatToClosestSkColorType(
      /*gpu_compositing=*/true, format());
  backend_texture_ = context_state_->gr_context()->createBackendTexture(
      size().width(), size().height(), sk_color, mipmap, GrRenderable::kYes,
      GrProtected::kNo);
  if (!backend_texture_.isValid()) {
    DLOG(ERROR) << "createBackendTexture() failed with SkColorType:"
                << sk_color;
    return false;
  }
  promise_texture_ = SkPromiseImageTexture::Make(backend_texture_);

  auto surface = SkSurface::MakeFromBackendTexture(
      context_state_->gr_context(), backend_texture_, surface_origin(),
      final_msaa_count_, sk_color, color_space().ToSkColorSpace(),
      &surface_props_);

  if (clear_color_)
    surface->getCanvas()->clear(*clear_color_);

  if (paint_op_buffer_) {
    cc::PlaybackParams playback_params(nullptr, SkM44());
    paint_op_buffer_->Playback(surface->getCanvas(), playback_params);
  }

  // Insert resolveMSAA in surface's command stream, so if the surface,
  // otherwise gr_context->flush() call will not resolve to the wrapped
  // backend_texture_.
  surface->resolveMSAA();

  return true;
}

void SharedImageBackingRawDraw::DestroyBackendTexture() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (backend_texture_.isValid()) {
    DCHECK(context_state_);
    DeleteGrBackendTexture(context_state_.get(), &backend_texture_);
    backend_texture_ = {};
    promise_texture_.reset();
  }
}

cc::PaintOpBuffer* SharedImageBackingRawDraw::BeginRasterWriteAccess(
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    const absl::optional<SkColor>& clear_color) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  AutoLock auto_lock(this);
  if (read_count_) {
    LOG(ERROR) << "The backing is being read.";
    return nullptr;
  }

  if (is_write_) {
    LOG(ERROR) << "The backing is being written.";
    return nullptr;
  }

  is_write_ = true;

  ResetPaintOpBuffer();
  // Should we keep the backing?
  DestroyBackendTexture();

  if (!paint_op_buffer_)
    paint_op_buffer_ = sk_make_sp<cc::PaintOpBuffer>();

  final_msaa_count_ = final_msaa_count;
  surface_props_ = surface_props;
  clear_color_ = clear_color;

  return paint_op_buffer_.get();
}

void SharedImageBackingRawDraw::EndRasterWriteAccess(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  AutoLock auto_lock(this);
  DCHECK_EQ(read_count_, 0);
  DCHECK(is_write_);

  is_write_ = false;

  if (callback) {
    DCHECK(!paint_op_release_callback_);
    paint_op_release_callback_ = std::move(callback);
  }
}

cc::PaintOpBuffer* SharedImageBackingRawDraw::BeginRasterReadAccess(
    absl::optional<SkColor>& clear_color) {
  // paint ops will be read on compositor thread, so do not check thread with
  // DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  AutoLock auto_lock(this);
  if (is_write_) {
    LOG(ERROR) << "The backing is being written.";
    return nullptr;
  }

  // If |backend_texture_| is valid, |paint_op_buffer_| should be played back
  // to the |backend_texture_| already, and |paint_op_buffer_| could be
  // released already. So we return nullptr here, and then SkiaRenderer will
  // fallback to using |backend_texture_|.
  if (backend_texture_.isValid())
    return nullptr;

  // If |paint_op_buffer_| contains SaveLayerOps, it usually means a SVG image
  // is drawn. For some complex SVG re-rasterizing is expensive, it causes
  // janky scrolling for some page which SVG images are heavily used.
  // Workaround the problem by return nullptr here, and then SkiaRenderer will
  // fallback to using |backing_texture_|.
  // TODO(crbug.com/1292068): only cache raster results for the SaveLayerOp
  // covered area.
  if (paint_op_buffer_ && paint_op_buffer_->has_save_layer_ops())
    return nullptr;

  read_count_++;

  if (!paint_op_buffer_) {
    paint_op_buffer_ = sk_make_sp<cc::PaintOpBuffer>();
  }

  clear_color = clear_color_;
  return paint_op_buffer_.get();
}

sk_sp<SkPromiseImageTexture> SharedImageBackingRawDraw::BeginSkiaReadAccess() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  AutoLock auto_lock(this);
  if (!backend_texture_.isValid() && !CreateBackendTextureAndFlushPaintOps())
    return nullptr;

  DCHECK(promise_texture_);
  read_count_++;
  return promise_texture_;
}

void SharedImageBackingRawDraw::EndReadAccess() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  AutoLock auto_lock(this);
  DCHECK_GE(read_count_, 0);
  DCHECK(!is_write_);
  read_count_--;

  // If the |backend_texture_| is valid, the |paint_op_buffer_| should have
  // been played back to the |backend_texture_| already, so we can release
  // the |paint_op_buffer_| now.
  if (read_count_ == 0 && backend_texture_.isValid())
    ResetPaintOpBuffer();
}

}  // namespace gpu
