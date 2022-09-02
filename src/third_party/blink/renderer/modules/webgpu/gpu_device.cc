// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

#include "gpu/command_buffer/client/webgpu_interface.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_compute_pipeline_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_device_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_error_filter.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_external_texture_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_feature_name.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_queue_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_render_pipeline_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_uncaptured_error_event_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_htmlvideoelement_videoframe.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_adapter.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group_layout.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_command_encoder.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_compute_pipeline.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device_lost_info.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_external_texture.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_out_of_memory_error.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_pipeline_layout.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_query_set.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_queue.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_render_bundle_encoder.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_render_pipeline.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_sampler.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_shader_module.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_supported_features.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_supported_limits.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_uncaptured_error_event.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_validation_error.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"

namespace blink {

namespace {

Vector<String> ToStringVector(const Vector<V8GPUFeatureName>& features) {
  Vector<String> str_features;
  for (auto&& feature : features)
    str_features.push_back(IDLEnumAsString(feature));
  return str_features;
}

}  // anonymous namespace

GPUDevice::GPUDevice(ExecutionContext* execution_context,
                     scoped_refptr<DawnControlClientHolder> dawn_control_client,
                     GPUAdapter* adapter,
                     WGPUDevice dawn_device,
                     const GPUDeviceDescriptor* descriptor)
    : ExecutionContextClient(execution_context),
      DawnObject(dawn_control_client, dawn_device),
      adapter_(adapter),
      features_(MakeGarbageCollected<GPUSupportedFeatures>(
          ToStringVector(descriptor->requiredFeatures()))),
      queue_(MakeGarbageCollected<GPUQueue>(
          this,
          GetProcs().deviceGetQueue(GetHandle()))),
      lost_property_(MakeGarbageCollected<LostProperty>(execution_context)),
      error_callback_(BindWGPURepeatingCallback(&GPUDevice::OnUncapturedError,
                                                WrapWeakPersistent(this))),
      logging_callback_(BindWGPURepeatingCallback(&GPUDevice::OnLogging,
                                                  WrapWeakPersistent(this))),
      // Note: This is a *repeating* callback even though we expect it to only
      // be called once. This is because it may be called *zero* times.
      // Because it might never be called, the GPUDevice needs to own the
      // allocation so it can be appropriately freed on destruction. Thus, the
      // callback should not be a OnceCallback which self-deletes after it is
      // called.
      lost_callback_(BindWGPURepeatingCallback(&GPUDevice::OnDeviceLostError,
                                               WrapWeakPersistent(this))) {
  DCHECK(dawn_device);

  WGPUSupportedLimits limits = {};
  GetProcs().deviceGetLimits(GetHandle(), &limits);
  limits_ = MakeGarbageCollected<GPUSupportedLimits>(limits);

  GetProcs().deviceSetUncapturedErrorCallback(
      GetHandle(), error_callback_->UnboundCallback(),
      error_callback_->AsUserdata());
  GetProcs().deviceSetLoggingCallback(GetHandle(),
                                      logging_callback_->UnboundCallback(),
                                      logging_callback_->AsUserdata());
  GetProcs().deviceSetDeviceLostCallback(GetHandle(),
                                         lost_callback_->UnboundCallback(),
                                         lost_callback_->AsUserdata());

  if (descriptor->hasLabel())
    setLabel(descriptor->label());

  if (descriptor->defaultQueue()->hasLabel())
    queue_->setLabel(descriptor->defaultQueue()->label());
}

GPUDevice::~GPUDevice() {
  // Perform destruction that's safe to do inside a GC (as in it doesn't touch
  // other GC objects).

  // Clear the callbacks since we can't handle callbacks after finalization.
  // error_callback_, logging_callback_, and lost_callback_ will be deleted.
  GetProcs().deviceSetUncapturedErrorCallback(GetHandle(), nullptr, nullptr);
  GetProcs().deviceSetLoggingCallback(GetHandle(), nullptr, nullptr);
  GetProcs().deviceSetDeviceLostCallback(GetHandle(), nullptr, nullptr);
}

void GPUDevice::InjectError(WGPUErrorType type, const char* message) {
  GetProcs().deviceInjectError(GetHandle(), type, message);
}

void GPUDevice::AddConsoleWarning(const char* message) {
  ExecutionContext* execution_context = GetExecutionContext();
  if (execution_context && allowed_console_warnings_remaining_ > 0) {
    auto* console_message = MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kRendering,
        mojom::blink::ConsoleMessageLevel::kWarning, message);
    execution_context->AddConsoleMessage(console_message);

    allowed_console_warnings_remaining_--;
    if (allowed_console_warnings_remaining_ == 0) {
      auto* final_message = MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kRendering,
          mojom::blink::ConsoleMessageLevel::kWarning,
          "WebGPU: too many warnings, no more warnings will be reported to the "
          "console for this GPUDevice.");
      execution_context->AddConsoleMessage(final_message);
    }
  }
}

// Validates that any features required for the given texture format are enabled
// for this device. If not, throw a TypeError to ensure consistency with
// browsers that haven't yet implemented the feature.
bool GPUDevice::ValidateTextureFormatUsage(V8GPUTextureFormat format,
                                           ExceptionState& exception_state) {
  const char* requiredFeature = nullptr;

  switch (format.AsEnum()) {
    case V8GPUTextureFormat::Enum::kBc1RgbaUnorm:
    case V8GPUTextureFormat::Enum::kBc1RgbaUnormSrgb:
    case V8GPUTextureFormat::Enum::kBc2RgbaUnorm:
    case V8GPUTextureFormat::Enum::kBc2RgbaUnormSrgb:
    case V8GPUTextureFormat::Enum::kBc3RgbaUnorm:
    case V8GPUTextureFormat::Enum::kBc3RgbaUnormSrgb:
    case V8GPUTextureFormat::Enum::kBc4RUnorm:
    case V8GPUTextureFormat::Enum::kBc4RSnorm:
    case V8GPUTextureFormat::Enum::kBc5RgUnorm:
    case V8GPUTextureFormat::Enum::kBc5RgSnorm:
    case V8GPUTextureFormat::Enum::kBc6HRgbUfloat:
    case V8GPUTextureFormat::Enum::kBc6HRgbFloat:
    case V8GPUTextureFormat::Enum::kBc7RgbaUnorm:
    case V8GPUTextureFormat::Enum::kBc7RgbaUnormSrgb:
      requiredFeature = "texture-compression-bc";
      break;

    case V8GPUTextureFormat::Enum::kEtc2Rgb8Unorm:
    case V8GPUTextureFormat::Enum::kEtc2Rgb8UnormSrgb:
    case V8GPUTextureFormat::Enum::kEtc2Rgb8A1Unorm:
    case V8GPUTextureFormat::Enum::kEtc2Rgb8A1UnormSrgb:
    case V8GPUTextureFormat::Enum::kEtc2Rgba8Unorm:
    case V8GPUTextureFormat::Enum::kEtc2Rgba8UnormSrgb:
    case V8GPUTextureFormat::Enum::kEacR11Unorm:
    case V8GPUTextureFormat::Enum::kEacR11Snorm:
    case V8GPUTextureFormat::Enum::kEacRg11Unorm:
    case V8GPUTextureFormat::Enum::kEacRg11Snorm:
      requiredFeature = "texture-compression-etc2";
      break;

    case V8GPUTextureFormat::Enum::kAstc4X4Unorm:
    case V8GPUTextureFormat::Enum::kAstc4X4UnormSrgb:
    case V8GPUTextureFormat::Enum::kAstc5X4Unorm:
    case V8GPUTextureFormat::Enum::kAstc5X4UnormSrgb:
    case V8GPUTextureFormat::Enum::kAstc5X5Unorm:
    case V8GPUTextureFormat::Enum::kAstc5X5UnormSrgb:
    case V8GPUTextureFormat::Enum::kAstc6X5Unorm:
    case V8GPUTextureFormat::Enum::kAstc6X5UnormSrgb:
    case V8GPUTextureFormat::Enum::kAstc6X6Unorm:
    case V8GPUTextureFormat::Enum::kAstc6X6UnormSrgb:
    case V8GPUTextureFormat::Enum::kAstc8X5Unorm:
    case V8GPUTextureFormat::Enum::kAstc8X5UnormSrgb:
    case V8GPUTextureFormat::Enum::kAstc8X6Unorm:
    case V8GPUTextureFormat::Enum::kAstc8X6UnormSrgb:
    case V8GPUTextureFormat::Enum::kAstc8X8Unorm:
    case V8GPUTextureFormat::Enum::kAstc8X8UnormSrgb:
    case V8GPUTextureFormat::Enum::kAstc10X5Unorm:
    case V8GPUTextureFormat::Enum::kAstc10X5UnormSrgb:
    case V8GPUTextureFormat::Enum::kAstc10X6Unorm:
    case V8GPUTextureFormat::Enum::kAstc10X6UnormSrgb:
    case V8GPUTextureFormat::Enum::kAstc10X8Unorm:
    case V8GPUTextureFormat::Enum::kAstc10X8UnormSrgb:
    case V8GPUTextureFormat::Enum::kAstc10X10Unorm:
    case V8GPUTextureFormat::Enum::kAstc10X10UnormSrgb:
    case V8GPUTextureFormat::Enum::kAstc12X10Unorm:
    case V8GPUTextureFormat::Enum::kAstc12X10UnormSrgb:
    case V8GPUTextureFormat::Enum::kAstc12X12Unorm:
    case V8GPUTextureFormat::Enum::kAstc12X12UnormSrgb:
      requiredFeature = "texture-compression-astc";
      break;

    default:
      return true;
  }

  DCHECK(requiredFeature != nullptr);

  if (features_->has(requiredFeature)) {
    return true;
  }

  std::string deviceLabel =
      label().IsEmpty() ? "" : " \"" + label().Utf8() + "\"";

  exception_state.ThrowTypeError(
      String::Format("Use of the '%s' texture format requires the '%s' feature "
                     "to be enabled on [Device%s].",
                     format.AsCStr(), requiredFeature, deviceLabel.c_str()));
  return false;
}

void GPUDevice::OnUncapturedError(WGPUErrorType errorType,
                                  const char* message) {
  // Suppress errors once the device is lost.
  if (lost_property_->GetState() == LostProperty::kResolved) {
    return;
  }

  DCHECK_NE(errorType, WGPUErrorType_NoError);
  DCHECK_NE(errorType, WGPUErrorType_DeviceLost);
  LOG(ERROR) << "GPUDevice: " << message;
  AddConsoleWarning(message);

  GPUUncapturedErrorEventInit* init = GPUUncapturedErrorEventInit::Create();
  if (errorType == WGPUErrorType_Validation) {
    init->setError(MakeGarbageCollected<GPUValidationError>(message));
  } else if (errorType == WGPUErrorType_OutOfMemory) {
    init->setError(MakeGarbageCollected<GPUOutOfMemoryError>(message));
  } else {
    return;
  }
  DispatchEvent(*GPUUncapturedErrorEvent::Create(
      event_type_names::kUncapturederror, init));
}

void GPUDevice::OnLogging(WGPULoggingType loggingType, const char* message) {
  // Callback function for WebGPU logging return command
  mojom::blink::ConsoleMessageLevel level;
  switch (loggingType) {
    case (WGPULoggingType_Verbose): {
      level = mojom::blink::ConsoleMessageLevel::kVerbose;
      break;
    }
    case (WGPULoggingType_Info): {
      level = mojom::blink::ConsoleMessageLevel::kInfo;
      break;
    }
    case (WGPULoggingType_Warning): {
      level = mojom::blink::ConsoleMessageLevel::kWarning;
      break;
    }
    case (WGPULoggingType_Error): {
      level = mojom::blink::ConsoleMessageLevel::kError;
      break;
    }
    default: {
      level = mojom::blink::ConsoleMessageLevel::kError;
      break;
    }
  }
  ExecutionContext* execution_context = GetExecutionContext();
  if (execution_context) {
    auto* console_message = MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kRendering, level, message);
    execution_context->AddConsoleMessage(console_message);
  }
}

void GPUDevice::OnDeviceLostError(WGPUDeviceLostReason reason,
                                  const char* message) {
  if (!GetExecutionContext())
    return;
  AddConsoleWarning(message);

  if (lost_property_->GetState() == LostProperty::kPending) {
    auto* device_lost_info =
        MakeGarbageCollected<GPUDeviceLostInfo>(reason, message);
    lost_property_->Resolve(device_lost_info);
  }
}

void GPUDevice::OnCreateRenderPipelineAsyncCallback(
    ScriptPromiseResolver* resolver,
    WGPUCreatePipelineAsyncStatus status,
    WGPURenderPipeline render_pipeline,
    const char* message) {
  switch (status) {
    case WGPUCreatePipelineAsyncStatus_Success: {
      resolver->Resolve(
          MakeGarbageCollected<GPURenderPipeline>(this, render_pipeline));
      break;
    }

    case WGPUCreatePipelineAsyncStatus_Error:
    case WGPUCreatePipelineAsyncStatus_DeviceLost:
    case WGPUCreatePipelineAsyncStatus_DeviceDestroyed:
    case WGPUCreatePipelineAsyncStatus_Unknown: {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kOperationError, message));
      break;
    }

    default: {
      NOTREACHED();
    }
  }
}

void GPUDevice::OnCreateComputePipelineAsyncCallback(
    ScriptPromiseResolver* resolver,
    WGPUCreatePipelineAsyncStatus status,
    WGPUComputePipeline compute_pipeline,
    const char* message) {
  switch (status) {
    case WGPUCreatePipelineAsyncStatus_Success: {
      resolver->Resolve(
          MakeGarbageCollected<GPUComputePipeline>(this, compute_pipeline));
      break;
    }

    case WGPUCreatePipelineAsyncStatus_Error:
    case WGPUCreatePipelineAsyncStatus_DeviceLost:
    case WGPUCreatePipelineAsyncStatus_DeviceDestroyed:
    case WGPUCreatePipelineAsyncStatus_Unknown: {
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kOperationError, message));
      break;
    }

    default: {
      NOTREACHED();
    }
  }
}

GPUAdapter* GPUDevice::adapter() const {
  return adapter_;
}

GPUSupportedFeatures* GPUDevice::features() const {
  return features_;
}

ScriptPromise GPUDevice::lost(ScriptState* script_state) {
  return lost_property_->Promise(script_state->World());
}

GPUQueue* GPUDevice::queue() {
  return queue_;
}

void GPUDevice::destroy() {
  destroyed_ = true;
  DestroyAllExternalTextures();
  GetProcs().deviceDestroy(GetHandle());
  FlushNow();
}

GPUBuffer* GPUDevice::createBuffer(const GPUBufferDescriptor* descriptor) {
  return GPUBuffer::Create(this, descriptor);
}

GPUTexture* GPUDevice::createTexture(const GPUTextureDescriptor* descriptor,
                                     ExceptionState& exception_state) {
  return GPUTexture::Create(this, descriptor, exception_state);
}

GPUTexture* GPUDevice::experimentalImportTexture(
    HTMLCanvasElement* canvas,
    unsigned int usage_flags,
    ExceptionState& exception_state) {
  return GPUTexture::FromCanvas(this, canvas,
                                static_cast<WGPUTextureUsage>(usage_flags),
                                exception_state);
}

GPUSampler* GPUDevice::createSampler(const GPUSamplerDescriptor* descriptor) {
  return GPUSampler::Create(this, descriptor);
}

GPUExternalTexture* GPUDevice::importExternalTexture(
    const GPUExternalTextureDescriptor* descriptor,
    ExceptionState& exception_state) {
  // Ensure the GPUExternalTexture created from a destroyed GPUDevice will be
  // expired immediately.
  if (destroyed_)
    return GPUExternalTexture::CreateExpired(this, descriptor, exception_state);

  return GPUExternalTexture::Create(this, descriptor, exception_state);
}

GPUBindGroup* GPUDevice::createBindGroup(
    const GPUBindGroupDescriptor* descriptor,
    ExceptionState& exception_state) {
  return GPUBindGroup::Create(this, descriptor, exception_state);
}

GPUBindGroupLayout* GPUDevice::createBindGroupLayout(
    const GPUBindGroupLayoutDescriptor* descriptor,
    ExceptionState& exception_state) {
  return GPUBindGroupLayout::Create(this, descriptor, exception_state);
}

GPUPipelineLayout* GPUDevice::createPipelineLayout(
    const GPUPipelineLayoutDescriptor* descriptor) {
  return GPUPipelineLayout::Create(this, descriptor);
}

GPUShaderModule* GPUDevice::createShaderModule(
    const GPUShaderModuleDescriptor* descriptor,
    ExceptionState& exception_state) {
  return GPUShaderModule::Create(this, descriptor, exception_state);
}

GPURenderPipeline* GPUDevice::createRenderPipeline(
    ScriptState* script_state,
    const GPURenderPipelineDescriptor* descriptor) {
  return GPURenderPipeline::Create(script_state, this, descriptor);
}

GPUComputePipeline* GPUDevice::createComputePipeline(
    const GPUComputePipelineDescriptor* descriptor,
    ExceptionState& exception_state) {
  return GPUComputePipeline::Create(this, descriptor);
}

ScriptPromise GPUDevice::createRenderPipelineAsync(
    ScriptState* script_state,
    const GPURenderPipelineDescriptor* descriptor) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  v8::Isolate* isolate = script_state->GetIsolate();
  ExceptionState exception_state(isolate, ExceptionState::kExecutionContext,
                                 "GPUDevice", "createRenderPipelineAsync");
  OwnedRenderPipelineDescriptor dawn_desc_info;
  ConvertToDawnType(isolate, this, descriptor, &dawn_desc_info,
                    exception_state);
  if (exception_state.HadException()) {
    resolver->Reject(exception_state);
  } else {
    auto* callback =
        BindWGPUOnceCallback(&GPUDevice::OnCreateRenderPipelineAsyncCallback,
                             WrapPersistent(this), WrapPersistent(resolver));
    GetProcs().deviceCreateRenderPipelineAsync(
        GetHandle(), &dawn_desc_info.dawn_desc, callback->UnboundCallback(),
        callback->AsUserdata());
  }

  // WebGPU guarantees that promises are resolved in finite time so we need to
  // ensure commands are flushed.
  EnsureFlush();
  return promise;
}

ScriptPromise GPUDevice::createComputePipelineAsync(
    ScriptState* script_state,
    const GPUComputePipelineDescriptor* descriptor) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  std::string label;
  OwnedProgrammableStageDescriptor computeStageDescriptor;
  WGPUComputePipelineDescriptor dawn_desc =
      AsDawnType(this, descriptor, &label, &computeStageDescriptor);

  auto* callback =
      BindWGPUOnceCallback(&GPUDevice::OnCreateComputePipelineAsyncCallback,
                           WrapPersistent(this), WrapPersistent(resolver));
  GetProcs().deviceCreateComputePipelineAsync(GetHandle(), &dawn_desc,
                                              callback->UnboundCallback(),
                                              callback->AsUserdata());
  // WebGPU guarantees that promises are resolved in finite time so we need to
  // ensure commands are flushed.
  EnsureFlush();
  return promise;
}

GPUCommandEncoder* GPUDevice::createCommandEncoder(
    const GPUCommandEncoderDescriptor* descriptor) {
  return GPUCommandEncoder::Create(this, descriptor);
}

GPURenderBundleEncoder* GPUDevice::createRenderBundleEncoder(
    const GPURenderBundleEncoderDescriptor* descriptor,
    ExceptionState& exception_state) {
  return GPURenderBundleEncoder::Create(this, descriptor, exception_state);
}

GPUQuerySet* GPUDevice::createQuerySet(
    const GPUQuerySetDescriptor* descriptor) {
  return GPUQuerySet::Create(this, descriptor);
}

void GPUDevice::pushErrorScope(const V8GPUErrorFilter& filter) {
  GetProcs().devicePushErrorScope(GetHandle(), AsDawnEnum(filter));
}

ScriptPromise GPUDevice::popErrorScope(ScriptState* script_state) {
  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  auto* callback =
      BindWGPUOnceCallback(&GPUDevice::OnPopErrorScopeCallback,
                           WrapPersistent(this), WrapPersistent(resolver));

  GetProcs().devicePopErrorScope(GetHandle(), callback->UnboundCallback(),
                                 callback->AsUserdata());

  // WebGPU guarantees that promises are resolved in finite time so we
  // need to ensure commands are flushed.
  EnsureFlush();
  return promise;
}

void GPUDevice::OnPopErrorScopeCallback(ScriptPromiseResolver* resolver,
                                        WGPUErrorType type,
                                        const char* message) {
  v8::Isolate* isolate = resolver->GetScriptState()->GetIsolate();
  switch (type) {
    case WGPUErrorType_NoError:
      resolver->Resolve(v8::Null(isolate));
      break;
    case WGPUErrorType_OutOfMemory:
      resolver->Resolve(MakeGarbageCollected<GPUOutOfMemoryError>(message));
      break;
    case WGPUErrorType_Validation:
      resolver->Resolve(MakeGarbageCollected<GPUValidationError>(message));
      break;
    case WGPUErrorType_Unknown:
    case WGPUErrorType_DeviceLost:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kOperationError));
      break;
    default:
      NOTREACHED();
  }
}

ExecutionContext* GPUDevice::GetExecutionContext() const {
  return ExecutionContextClient::GetExecutionContext();
}

const AtomicString& GPUDevice::InterfaceName() const {
  return event_target_names::kGPUDevice;
}

void GPUDevice::Trace(Visitor* visitor) const {
  visitor->Trace(adapter_);
  visitor->Trace(features_);
  visitor->Trace(limits_);
  visitor->Trace(queue_);
  visitor->Trace(lost_property_);
  visitor->Trace(active_external_textures_);
  visitor->Trace(external_textures_pending_destroy_);
  ExecutionContextClient::Trace(visitor);
  EventTargetWithInlineData::Trace(visitor);
}

void GPUDevice::EnsureExternalTextureDestroyed(
    GPUExternalTexture* externalTexture) {
  DCHECK(externalTexture);
  external_textures_pending_destroy_.push_back(externalTexture);

  if (has_destroy_external_texture_microtask_)
    return;

  Microtask::EnqueueMicrotask(WTF::Bind(
      &GPUDevice::DestroyExternalTexturesMicrotask, WrapWeakPersistent(this)));
  has_destroy_external_texture_microtask_ = true;
}

void GPUDevice::DestroyExternalTexturesMicrotask() {
  // GPUDevice.destroy() call has destroyed all pending external textures.
  if (!has_destroy_external_texture_microtask_)
    return;

  has_destroy_external_texture_microtask_ = false;

  auto externalTextures = std::move(external_textures_pending_destroy_);
  for (Member<GPUExternalTexture> externalTexture : externalTextures) {
    externalTexture->Destroy();
  }
}

void GPUDevice::Dispose() {
  // This call accesses other GC objects, so it cannot be called inside GC
  // objects destructors. Instead call it in the pre-finalizer.
  DestroyAllExternalTextures();
}

void GPUDevice::DestroyAllExternalTextures() {
  has_destroy_external_texture_microtask_ = false;

  // TODO(crbug.com/1318345): u-nit: fix casing issues in all GPUExternalTexture
  // related codes. Using a_b instead of aB.
  for (auto& externalTexture : active_external_textures_) {
    externalTexture->Destroy();
  }
  active_external_textures_.clear();

  auto externalTexturesPendingDestroy =
      std::move(external_textures_pending_destroy_);
  for (Member<GPUExternalTexture> externalTexture :
       externalTexturesPendingDestroy) {
    externalTexture->Destroy();
  }
}

void GPUDevice::AddActiveExternalTexture(GPUExternalTexture* external_texture) {
  DCHECK(external_texture);
  active_external_textures_.insert(external_texture);
}

void GPUDevice::RemoveActiveExternalTexture(
    GPUExternalTexture* external_texture) {
  DCHECK(external_texture);
  active_external_textures_.erase(external_texture);
}

}  // namespace blink
