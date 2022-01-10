// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_mailbox_texture.h"

#include "gpu/command_buffer/client/webgpu_interface.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"

namespace blink {

// static
scoped_refptr<WebGPUMailboxTexture> WebGPUMailboxTexture::FromStaticBitmapImage(
    scoped_refptr<DawnControlClientHolder> dawn_control_client,
    WGPUDevice device,
    WGPUTextureUsage usage,
    scoped_refptr<StaticBitmapImage> image,
    CanvasColorSpace color_space,
    SkColorType color_type) {
  DCHECK(image->IsTextureBacked());

  // TODO(crbugs.com/1217160) Mac uses IOSurface in SharedImageBackingGLImage
  // which can be shared to dawn directly aftter passthrough command buffer
  // supported on mac os.
  // We should wrap the StaticBitmapImage directly for mac when passthrough
  // command buffer has been supported.

  // If the context is lost, the resource provider would be invalid.
  auto context_provider_wrapper = SharedGpuContext::ContextProviderWrapper();
  if (!context_provider_wrapper ||
      context_provider_wrapper->ContextProvider()->IsContextLost())
    return nullptr;

  // Keep the same config as source image.
  SkImageInfo info = SkImageInfo::Make(
      image->Size().width(), image->Size().height(), color_type,
      image->IsPremultiplied() ? kPremul_SkAlphaType : kUnpremul_SkAlphaType,
      CanvasColorSpaceToSkColorSpace(color_space));

  // Get a recyclable resource for producing WebGPU-compatible shared images.
  std::unique_ptr<RecyclableCanvasResource> recyclable_canvas_resource =
      dawn_control_client->GetOrCreateCanvasResource(info,
                                                     image->IsOriginTopLeft());

  // Fallback to unstable intermediate resource copy path.
  if (!recyclable_canvas_resource) {
    auto finished_access_callback =
        WTF::Bind(&StaticBitmapImage::UpdateSyncToken, WTF::RetainedRef(image));

    return base::AdoptRef(new WebGPUMailboxTexture(
        std::move(dawn_control_client), device, usage,
        image->GetMailboxHolder().mailbox, image->GetMailboxHolder().sync_token,
        gpu::webgpu::WEBGPU_MAILBOX_NONE, std::move(finished_access_callback),
        /*recyclable_canvas_resource=*/nullptr));
  }

  CanvasResourceProvider* resource_provider =
      recyclable_canvas_resource->resource_provider();
  DCHECK(resource_provider);

  if (!image->CopyToResourceProvider(resource_provider)) {
    return nullptr;
  }

  return WebGPUMailboxTexture::FromCanvasResource(
      dawn_control_client, device, usage,
      std::move(recyclable_canvas_resource));
}

// static
scoped_refptr<WebGPUMailboxTexture> WebGPUMailboxTexture::FromCanvasResource(
    scoped_refptr<DawnControlClientHolder> dawn_control_client,
    WGPUDevice device,
    WGPUTextureUsage usage,
    std::unique_ptr<RecyclableCanvasResource> recyclable_canvas_resource) {
  scoped_refptr<CanvasResource> canvas_resource =
      recyclable_canvas_resource->resource_provider()->ProduceCanvasResource();
  DCHECK(canvas_resource->IsValid());
  DCHECK(canvas_resource->IsAccelerated());

  const gpu::Mailbox& mailbox =
      canvas_resource->GetOrCreateGpuMailbox(kUnverifiedSyncToken);
  gpu::SyncToken sync_token = canvas_resource->GetSyncToken();
  return base::AdoptRef(new WebGPUMailboxTexture(
      std::move(dawn_control_client), device, usage, mailbox, sync_token,
      gpu::webgpu::WEBGPU_MAILBOX_NONE,
      base::OnceCallback<void(const gpu::SyncToken&)>(),
      std::move(recyclable_canvas_resource)));
}

// static
scoped_refptr<WebGPUMailboxTexture> WebGPUMailboxTexture::FromExistingMailbox(
    scoped_refptr<DawnControlClientHolder> dawn_control_client,
    WGPUDevice device,
    WGPUTextureUsage usage,
    const gpu::Mailbox& mailbox,
    const gpu::SyncToken& sync_token,
    gpu::webgpu::MailboxFlags mailbox_flags) {
  DCHECK(dawn_control_client->GetContextProviderWeakPtr());

  return base::AdoptRef(new WebGPUMailboxTexture(
      std::move(dawn_control_client), device, usage, mailbox, sync_token,
      mailbox_flags, base::OnceCallback<void(const gpu::SyncToken&)>(),
      nullptr));
}

WebGPUMailboxTexture::WebGPUMailboxTexture(
    scoped_refptr<DawnControlClientHolder> dawn_control_client,
    WGPUDevice device,
    WGPUTextureUsage usage,
    const gpu::Mailbox& mailbox,
    const gpu::SyncToken& sync_token,
    gpu::webgpu::MailboxFlags mailbox_flags,
    base::OnceCallback<void(const gpu::SyncToken&)> destroy_callback,
    std::unique_ptr<RecyclableCanvasResource> recyclable_canvas_resource)
    : dawn_control_client_(std::move(dawn_control_client)),
      device_(device),
      destroy_callback_(std::move(destroy_callback)),
      recyclable_canvas_resource_(std::move(recyclable_canvas_resource)) {
  DCHECK(dawn_control_client_->GetContextProviderWeakPtr());

  dawn_control_client_->GetProcs().deviceReference(device_);

  gpu::webgpu::WebGPUInterface* webgpu =
      dawn_control_client_->GetContextProviderWeakPtr()
          ->ContextProvider()
          ->WebGPUInterface();

  // Wait on any work using the image.
  webgpu->WaitSyncTokenCHROMIUM(sync_token.GetConstData());

  // Produce and inject image to WebGPU texture
  gpu::webgpu::ReservedTexture reservation = webgpu->ReserveTexture(device_);
  DCHECK(reservation.texture);

  wire_texture_id_ = reservation.id;
  wire_texture_generation_ = reservation.generation;
  texture_ = reservation.texture;

  // This may fail because gl_backing resource cannot produce dawn
  // representation.
  webgpu->AssociateMailbox(reservation.deviceId, reservation.deviceGeneration,
                           wire_texture_id_, wire_texture_generation_, usage,
                           mailbox_flags,
                           reinterpret_cast<const GLbyte*>(&mailbox));
}

WebGPUMailboxTexture::~WebGPUMailboxTexture() {
  DCHECK_NE(wire_texture_id_, 0u);

  if (auto context_provider =
          dawn_control_client_->GetContextProviderWeakPtr()) {
    gpu::webgpu::WebGPUInterface* webgpu =
        context_provider->ContextProvider()->WebGPUInterface();
    webgpu->DissociateMailbox(wire_texture_id_, wire_texture_generation_);

    if (destroy_callback_) {
      gpu::SyncToken finished_access_token;
      webgpu->GenUnverifiedSyncTokenCHROMIUM(finished_access_token.GetData());
      std::move(destroy_callback_).Run(finished_access_token);
    }
    dawn_control_client_->GetProcs().textureRelease(texture_);
    dawn_control_client_->GetProcs().deviceRelease(device_);
  }
}

}  // namespace blink
