// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_DEVICE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_DEVICE_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_property.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"
#include "third_party/blink/renderer/platform/graphics/gpu/dawn_callback.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class ExecutionContext;
class HTMLCanvasElement;
class GPUAdapter;
class GPUAdapter;
class GPUBuffer;
class GPUBufferDescriptor;
class GPUCommandEncoder;
class GPUCommandEncoderDescriptor;
class GPUBindGroup;
class GPUBindGroupDescriptor;
class GPUBindGroupLayout;
class GPUBindGroupLayoutDescriptor;
class GPUComputePipeline;
class GPUComputePipelineDescriptor;
class GPUDeviceDescriptor;
class GPUDeviceLostInfo;
class GPUExternalTexture;
class GPUExternalTextureDescriptor;
class GPUPipelineLayout;
class GPUPipelineLayoutDescriptor;
class GPUQuerySet;
class GPUQuerySetDescriptor;
class GPUQueue;
class GPURenderBundleEncoder;
class GPURenderBundleEncoderDescriptor;
class GPURenderPipeline;
class GPURenderPipelineDescriptor;
class GPUSampler;
class GPUSamplerDescriptor;
class GPUShaderModule;
class GPUShaderModuleDescriptor;
class GPUSupportedFeatures;
class GPUSupportedLimits;
class GPUTexture;
class GPUTextureDescriptor;
class ScriptPromiseResolver;
class ScriptState;

class GPUDevice final : public EventTargetWithInlineData,
                        public ExecutionContextClient,
                        public DawnObject<WGPUDevice> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit GPUDevice(ExecutionContext* execution_context,
                     scoped_refptr<DawnControlClientHolder> dawn_control_client,
                     GPUAdapter* adapter,
                     WGPUDevice dawn_device,
                     const WGPUSupportedLimits* limits,
                     const GPUDeviceDescriptor* descriptor);

  GPUDevice(const GPUDevice&) = delete;
  GPUDevice& operator=(const GPUDevice&) = delete;

  ~GPUDevice() override;

  void Trace(Visitor* visitor) const override;

  // gpu_device.idl
  GPUAdapter* adapter() const;
  GPUSupportedFeatures* features() const;
  GPUSupportedLimits* limits() const { return limits_; }
  ScriptPromise lost(ScriptState* script_state);

  GPUQueue* queue();

  void destroy();

  GPUBuffer* createBuffer(const GPUBufferDescriptor* descriptor);
  GPUTexture* createTexture(const GPUTextureDescriptor* descriptor,
                            ExceptionState& exception_state);
  GPUTexture* experimentalImportTexture(HTMLCanvasElement* canvas,
                                        unsigned int usage_flags,
                                        ExceptionState& exception_state);
  GPUSampler* createSampler(const GPUSamplerDescriptor* descriptor);
  GPUExternalTexture* importExternalTexture(
      const GPUExternalTextureDescriptor* descriptor,
      ExceptionState& exception_state);

  GPUBindGroup* createBindGroup(const GPUBindGroupDescriptor* descriptor,
                                ExceptionState& exception_state);
  GPUBindGroupLayout* createBindGroupLayout(
      const GPUBindGroupLayoutDescriptor* descriptor,
      ExceptionState& exception_state);
  GPUPipelineLayout* createPipelineLayout(
      const GPUPipelineLayoutDescriptor* descriptor);

  GPUShaderModule* createShaderModule(
      const GPUShaderModuleDescriptor* descriptor,
      ExceptionState& exception_state);
  GPURenderPipeline* createRenderPipeline(
      ScriptState* script_state,
      const GPURenderPipelineDescriptor* descriptor);
  GPUComputePipeline* createComputePipeline(
      const GPUComputePipelineDescriptor* descriptor,
      ExceptionState& exception_state);
  ScriptPromise createRenderPipelineAsync(
      ScriptState* script_state,
      const GPURenderPipelineDescriptor* descriptor);
  ScriptPromise createComputePipelineAsync(
      ScriptState* script_state,
      const GPUComputePipelineDescriptor* descriptor);

  GPUCommandEncoder* createCommandEncoder(
      const GPUCommandEncoderDescriptor* descriptor);
  GPURenderBundleEncoder* createRenderBundleEncoder(
      const GPURenderBundleEncoderDescriptor* descriptor);

  GPUQuerySet* createQuerySet(const GPUQuerySetDescriptor* descriptor);

  void pushErrorScope(const WTF::String& filter);
  ScriptPromise popErrorScope(ScriptState* script_state);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(uncapturederror, kUncapturederror)

  // EventTarget overrides.
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  void InjectError(WGPUErrorType type, const char* message);
  void AddConsoleWarning(const char* message);

  void EnsureExternalTextureDestroyed(GPUExternalTexture* externalTexture);

 private:
  using LostProperty =
      ScriptPromiseProperty<Member<GPUDeviceLostInfo>, ToV8UndefinedGenerator>;

  void DestroyExternalTexturesMicrotask();

  void OnUncapturedError(WGPUErrorType errorType, const char* message);
  void OnLogging(WGPULoggingType loggingType, const char* message);
  void OnDeviceLostError(WGPUDeviceLostReason, const char* message);

  void OnPopErrorScopeCallback(ScriptPromiseResolver* resolver,
                               WGPUErrorType type,
                               const char* message);

  void OnCreateRenderPipelineAsyncCallback(ScriptPromiseResolver* resolver,
                                           WGPUCreatePipelineAsyncStatus status,
                                           WGPURenderPipeline render_pipeline,
                                           const char* message);
  void OnCreateComputePipelineAsyncCallback(
      ScriptPromiseResolver* resolver,
      WGPUCreatePipelineAsyncStatus status,
      WGPUComputePipeline compute_pipeline,
      const char* message);

  Member<GPUAdapter> adapter_;
  Member<GPUSupportedFeatures> features_;
  Member<GPUSupportedLimits> limits_;
  Member<GPUQueue> queue_;
  Member<LostProperty> lost_property_;
  std::unique_ptr<DawnRepeatingCallback<void(WGPUErrorType, const char*)>>
      error_callback_;
  std::unique_ptr<DawnRepeatingCallback<void(WGPULoggingType, const char*)>>
      logging_callback_;
  // lost_callback_ is stored as a unique_ptr since it may never be called.
  // We need to be sure to free it on deletion of the device.
  // Inside OnDeviceLostError we'll release the unique_ptr to avoid a double
  // free.
  std::unique_ptr<
      DawnRepeatingCallback<void(WGPUDeviceLostReason, const char*)>>
      lost_callback_;

  static constexpr int kMaxAllowedConsoleWarnings = 500;
  int allowed_console_warnings_remaining_ = kMaxAllowedConsoleWarnings;

  bool has_pending_microtask_ = false;
  HeapVector<Member<GPUExternalTexture>> external_textures_pending_destroy_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_DEVICE_H_
