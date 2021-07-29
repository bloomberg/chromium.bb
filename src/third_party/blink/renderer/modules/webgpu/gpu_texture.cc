// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"

#include "gpu/command_buffer/client/webgpu_interface.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_texture_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_texture_view_descriptor.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture_usage.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture_view.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_mailbox_texture.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_resource_provider_cache.h"

namespace blink {

namespace {

WGPUTextureDescriptor AsDawnType(const GPUTextureDescriptor* webgpu_desc,
                                 std::string* label,
                                 GPUDevice* device) {
  DCHECK(webgpu_desc);
  DCHECK(label);
  DCHECK(device);

  WGPUTextureDescriptor dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  dawn_desc.usage = static_cast<WGPUTextureUsage>(webgpu_desc->usage());
  dawn_desc.dimension =
      AsDawnEnum<WGPUTextureDimension>(webgpu_desc->dimension());
  dawn_desc.size = AsDawnType(webgpu_desc->size());
  dawn_desc.format = AsDawnEnum<WGPUTextureFormat>(webgpu_desc->format());
  dawn_desc.mipLevelCount = webgpu_desc->mipLevelCount();
  dawn_desc.sampleCount = webgpu_desc->sampleCount();

  if (webgpu_desc->hasLabel()) {
    *label = webgpu_desc->label().Utf8();
    dawn_desc.label = label->c_str();
  }

  return dawn_desc;
}

WGPUTextureViewDescriptor AsDawnType(
    const GPUTextureViewDescriptor* webgpu_desc,
    std::string* label) {
  DCHECK(webgpu_desc);
  DCHECK(label);

  WGPUTextureViewDescriptor dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  if (webgpu_desc->hasFormat()) {
    dawn_desc.format = AsDawnEnum<WGPUTextureFormat>(webgpu_desc->format());
  }
  if (webgpu_desc->hasDimension()) {
    dawn_desc.dimension =
        AsDawnEnum<WGPUTextureViewDimension>(webgpu_desc->dimension());
  }
  dawn_desc.baseMipLevel = webgpu_desc->baseMipLevel();
  dawn_desc.mipLevelCount = webgpu_desc->mipLevelCount();
  dawn_desc.baseArrayLayer = webgpu_desc->baseArrayLayer();
  dawn_desc.arrayLayerCount = webgpu_desc->arrayLayerCount();
  dawn_desc.aspect = AsDawnEnum<WGPUTextureAspect>(webgpu_desc->aspect());
  if (webgpu_desc->hasLabel()) {
    *label = webgpu_desc->label().Utf8();
    dawn_desc.label = label->c_str();
  }

  return dawn_desc;
}

void popErrorDiscardCallback(WGPUErrorType, const char*, void*) {
  // This callback is used to silently consume expected error messages
}

}  // anonymous namespace

// static
GPUTexture* GPUTexture::Create(GPUDevice* device,
                               const GPUTextureDescriptor* webgpu_desc,
                               ExceptionState& exception_state) {
  DCHECK(device);
  DCHECK(webgpu_desc);

  std::string label;
  WGPUTextureDescriptor dawn_desc = AsDawnType(webgpu_desc, &label, device);

  GPUTexture* texture = MakeGarbageCollected<GPUTexture>(
      device,
      device->GetProcs().deviceCreateTexture(device->GetHandle(), &dawn_desc),
      dawn_desc.dimension, dawn_desc.format,
      static_cast<WGPUTextureUsage>(dawn_desc.usage));
  if (webgpu_desc->hasLabel())
    texture->setLabel(webgpu_desc->label());
  return texture;
}

// static
GPUTexture* GPUTexture::CreateError(GPUDevice* device) {
  DCHECK(device);

  // Force the creation of an invalid texture and consume the errors that it
  // causes. It would be nice if Dawn provided a more direct way of creating
  // an error texture to simplify this.
  WGPUTextureDescriptor dawn_desc = {};
  device->GetProcs().devicePushErrorScope(device->GetHandle(),
                                          WGPUErrorFilter_Validation);
  GPUTexture* texture = MakeGarbageCollected<GPUTexture>(
      device,
      device->GetProcs().deviceCreateTexture(device->GetHandle(), &dawn_desc),
      dawn_desc.dimension, dawn_desc.format,
      static_cast<WGPUTextureUsage>(dawn_desc.usage));
  device->GetProcs().devicePopErrorScope(device->GetHandle(),
                                         &popErrorDiscardCallback, nullptr);

  return texture;
}

// static
GPUTexture* GPUTexture::FromCanvas(GPUDevice* device,
                                   HTMLCanvasElement* canvas,
                                   WGPUTextureUsage usage,
                                   ExceptionState& exception_state) {
  if (!canvas || !canvas->width() || !canvas->height()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Missing canvas source");
    return nullptr;
  }

  if (!canvas->OriginClean()) {
    exception_state.ThrowSecurityError(
        "Canvas element is tainted by cross-origin data and may not be "
        "loaded.");
    return nullptr;
  }

  // TODO: Webgpu contexts also return true for Is3d(), but most of the webgl
  // specific CanvasRenderingContext methods don't work for webgpu.
  auto* canvas_context = canvas->RenderingContext();
  if (!canvas_context) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Missing canvas rendering context");
    return nullptr;
  }

  // If the context is lost, the resource provider would be invalid.
  auto context_provider_wrapper = SharedGpuContext::ContextProviderWrapper();
  if (!context_provider_wrapper ||
      context_provider_wrapper->ContextProvider()->IsContextLost()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Shared GPU context lost");
    return nullptr;
  }

  const CanvasResourceParams params(CanvasColorSpace::kSRGB, kN32_SkColorType,
                                    kPremul_SkAlphaType);

  // Get a recyclable resource for producing WebGPU-compatible shared images.
  // First texel i.e. UV (0, 0) should be mapped to top left of the source.
  std::unique_ptr<RecyclableCanvasResource> recyclable_canvas_resource =
      device->GetDawnControlClient()->GetOrCreateCanvasResource(
          canvas->Size(), params, /*is_origin_top_left=*/true);
  if (!recyclable_canvas_resource) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Failed to create resource provider");
    return nullptr;
  }

  CanvasResourceProvider* resource_provider =
      recyclable_canvas_resource->resource_provider();
  DCHECK(resource_provider);

  // Extract the format. This is only used to validate experimentalImportTexture
  // right now. We may want to reflect it from this function or validate it
  // against some input parameters.
  WGPUTextureFormat format =
      AsDawnType(resource_provider->ColorParams().GetSkColorType());
  if (format == WGPUTextureFormat_Undefined) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Unsupported format for import texture");
    return nullptr;
  }

  if (!canvas_context->CopyRenderingResultsFromDrawingBuffer(resource_provider,
                                                             kBackBuffer)) {
    // Fallback to static bitmap image.
    SourceImageStatus source_image_status = kInvalidSourceImageStatus;
    auto image = canvas->GetSourceImageForCanvas(&source_image_status,
                                                 FloatSize(canvas->Size()));
    if (source_image_status != kNormalSourceImageStatus) {
      exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                        "Failed to get image from canvas");
      return nullptr;
    }
    auto* static_bitmap_image = DynamicTo<StaticBitmapImage>(image.get());
    if (!static_bitmap_image ||
        !static_bitmap_image->CopyToResourceProvider(resource_provider)) {
      exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                        "Failed to import texture from canvas");
      return nullptr;
    }
  }

  scoped_refptr<WebGPUMailboxTexture> mailbox_texture =
      WebGPUMailboxTexture::FromCanvasResource(
          device->GetDawnControlClient(), device->GetHandle(), usage,
          std::move(recyclable_canvas_resource));
  DCHECK(mailbox_texture->GetTexture());

  return MakeGarbageCollected<GPUTexture>(device, format, usage,
                                          std::move(mailbox_texture));
}

GPUTexture::GPUTexture(GPUDevice* device,
                       WGPUTexture texture,
                       WGPUTextureDimension dimension,
                       WGPUTextureFormat format,
                       WGPUTextureUsage usage)
    : DawnObject<WGPUTexture>(device, texture),
      dimension_(dimension),
      format_(format),
      usage_(usage) {}

GPUTexture::GPUTexture(GPUDevice* device,
                       WGPUTextureFormat format,
                       WGPUTextureUsage usage,
                       scoped_refptr<WebGPUMailboxTexture> mailbox_texture)
    : DawnObject<WGPUTexture>(device, mailbox_texture->GetTexture()),
      format_(format),
      usage_(usage),
      mailbox_texture_(std::move(mailbox_texture)) {
  // Mailbox textures are all 2d texture.
  dimension_ = WGPUTextureDimension_2D;

  // The mailbox texture releases the texture on destruction, so reference it
  // here.
  GetProcs().textureReference(GetHandle());
}

GPUTextureView* GPUTexture::createView(
    const GPUTextureViewDescriptor* webgpu_desc) {
  DCHECK(webgpu_desc);

  std::string label;
  WGPUTextureViewDescriptor dawn_desc = AsDawnType(webgpu_desc, &label);
  GPUTextureView* view = MakeGarbageCollected<GPUTextureView>(
      device_, GetProcs().textureCreateView(GetHandle(), &dawn_desc));
  if (webgpu_desc->hasLabel())
    view->setLabel(webgpu_desc->label());
  return view;
}

void GPUTexture::destroy() {
  GetProcs().textureDestroy(GetHandle());
  mailbox_texture_.reset();
}

}  // namespace blink
