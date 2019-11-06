// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_render_pipeline.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_vertex_buffer_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_blend_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_color_state_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_depth_stencil_state_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_pipeline_layout.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_render_pipeline_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_vertex_buffer_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_vertex_input_descriptor.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

namespace {

DawnBlendDescriptor AsDawnType(const GPUBlendDescriptor* webgpu_desc) {
  DCHECK(webgpu_desc);

  DawnBlendDescriptor dawn_desc;
  dawn_desc.dstFactor = AsDawnEnum<DawnBlendFactor>(webgpu_desc->dstFactor());
  dawn_desc.srcFactor = AsDawnEnum<DawnBlendFactor>(webgpu_desc->srcFactor());
  dawn_desc.operation =
      AsDawnEnum<DawnBlendOperation>(webgpu_desc->operation());

  return dawn_desc;
}

}  // anonymous namespace

DawnColorStateDescriptor AsDawnType(
    const GPUColorStateDescriptor* webgpu_desc) {
  DCHECK(webgpu_desc);

  DawnColorStateDescriptor dawn_desc;
  dawn_desc.nextInChain = nullptr;
  dawn_desc.alphaBlend = AsDawnType(webgpu_desc->alphaBlend());
  dawn_desc.colorBlend = AsDawnType(webgpu_desc->colorBlend());
  dawn_desc.writeMask =
      AsDawnEnum<DawnColorWriteMask>(webgpu_desc->writeMask());
  dawn_desc.format = AsDawnEnum<DawnTextureFormat>(webgpu_desc->format());

  return dawn_desc;
}

namespace {

DawnStencilStateFaceDescriptor AsDawnType(
    const GPUStencilStateFaceDescriptor* webgpu_desc) {
  DCHECK(webgpu_desc);

  DawnStencilStateFaceDescriptor dawn_desc;
  dawn_desc.compare = AsDawnEnum<DawnCompareFunction>(webgpu_desc->compare());
  dawn_desc.depthFailOp =
      AsDawnEnum<DawnStencilOperation>(webgpu_desc->depthFailOp());
  dawn_desc.failOp = AsDawnEnum<DawnStencilOperation>(webgpu_desc->failOp());
  dawn_desc.passOp = AsDawnEnum<DawnStencilOperation>(webgpu_desc->passOp());

  return dawn_desc;
}

DawnDepthStencilStateDescriptor AsDawnType(
    const GPUDepthStencilStateDescriptor* webgpu_desc) {
  DCHECK(webgpu_desc);

  DawnDepthStencilStateDescriptor dawn_desc;
  dawn_desc.nextInChain = nullptr;
  dawn_desc.depthCompare =
      AsDawnEnum<DawnCompareFunction>(webgpu_desc->depthCompare());
  dawn_desc.depthWriteEnabled = webgpu_desc->depthWriteEnabled();
  dawn_desc.format = AsDawnEnum<DawnTextureFormat>(webgpu_desc->format());
  dawn_desc.stencilBack = AsDawnType(webgpu_desc->stencilBack());
  dawn_desc.stencilFront = AsDawnType(webgpu_desc->stencilFront());
  dawn_desc.stencilReadMask = webgpu_desc->stencilReadMask();
  dawn_desc.stencilWriteMask = webgpu_desc->stencilWriteMask();

  return dawn_desc;
}

using DawnVertexInputInfo = std::tuple<DawnVertexInputDescriptor,
                                       Vector<DawnVertexBufferDescriptor>,
                                       Vector<DawnVertexAttributeDescriptor>>;

DawnVertexInputInfo GPUVertexInputAsDawnInputState(
    v8::Isolate* isolate,
    const GPUVertexInputDescriptor* descriptor,
    ExceptionState& exception_state) {
  DawnVertexInputDescriptor dawn_desc;
  dawn_desc.indexFormat =
      AsDawnEnum<DawnIndexFormat>(descriptor->indexFormat());
  dawn_desc.numAttributes = 0;
  dawn_desc.attributes = nullptr;
  dawn_desc.numBuffers = 0;
  dawn_desc.buffers = nullptr;

  Vector<DawnVertexBufferDescriptor> dawn_vertex_buffers;
  Vector<DawnVertexAttributeDescriptor> dawn_vertex_attributes;

  if (descriptor->hasVertexBuffers()) {
    // TODO(crbug.com/951629): Use a sequence of nullable descriptors.
    v8::Local<v8::Value> vertex_buffers_value =
        descriptor->vertexBuffers().V8Value();
    if (!vertex_buffers_value->IsArray()) {
      exception_state.ThrowTypeError("vertexBuffers must be an array");

      return std::make_tuple(dawn_desc, std::move(dawn_vertex_buffers),
                             std::move(dawn_vertex_attributes));
    }

    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    v8::Local<v8::Array> vertex_buffers = vertex_buffers_value.As<v8::Array>();
    for (uint32_t i = 0; i < vertex_buffers->Length(); ++i) {
      // This array can be sparse. Skip empty slots.
      v8::MaybeLocal<v8::Value> maybe_value = vertex_buffers->Get(context, i);
      v8::Local<v8::Value> value;
      if (!maybe_value.ToLocal(&value) || value.IsEmpty() ||
          value->IsNullOrUndefined()) {
        continue;
      }

      GPUVertexBufferDescriptor vertex_buffer;
      V8GPUVertexBufferDescriptor::ToImpl(isolate, value, &vertex_buffer,
                                          exception_state);
      if (exception_state.HadException()) {
        return std::make_tuple(dawn_desc, std::move(dawn_vertex_buffers),
                               std::move(dawn_vertex_attributes));
      }

      DawnVertexBufferDescriptor dawn_vertex_buffer;
      dawn_vertex_buffer.inputSlot = i;
      dawn_vertex_buffer.stride = vertex_buffer.stride();
      dawn_vertex_buffer.stepMode =
          AsDawnEnum<DawnInputStepMode>(vertex_buffer.stepMode());
      dawn_vertex_buffers.push_back(dawn_vertex_buffer);

      for (wtf_size_t j = 0; j < vertex_buffer.attributes().size(); ++j) {
        const GPUVertexAttributeDescriptor* attribute =
            vertex_buffer.attributes()[j];
        DawnVertexAttributeDescriptor dawn_vertex_attribute;
        dawn_vertex_attribute.shaderLocation = attribute->shaderLocation();
        dawn_vertex_attribute.inputSlot = i;
        dawn_vertex_attribute.offset = attribute->offset();
        dawn_vertex_attribute.format =
            AsDawnEnum<DawnVertexFormat>(attribute->format());
        dawn_vertex_attributes.push_back(dawn_vertex_attribute);
      }
    }
  }

  dawn_desc.numAttributes =
      static_cast<uint32_t>(dawn_vertex_attributes.size());
  dawn_desc.attributes = dawn_vertex_attributes.data();
  dawn_desc.numBuffers = static_cast<uint32_t>(dawn_vertex_buffers.size());
  dawn_desc.buffers = dawn_vertex_buffers.data();

  return std::make_tuple(dawn_desc, std::move(dawn_vertex_buffers),
                         std::move(dawn_vertex_attributes));
}

DawnRasterizationStateDescriptor AsDawnType(
    const GPURasterizationStateDescriptor* webgpu_desc) {
  DCHECK(webgpu_desc);

  DawnRasterizationStateDescriptor dawn_desc;
  dawn_desc.nextInChain = nullptr;
  dawn_desc.frontFace = AsDawnEnum<DawnFrontFace>(webgpu_desc->frontFace());
  dawn_desc.cullMode = AsDawnEnum<DawnCullMode>(webgpu_desc->cullMode());
  dawn_desc.depthBias = webgpu_desc->depthBias();
  dawn_desc.depthBiasSlopeScale = webgpu_desc->depthBiasSlopeScale();
  dawn_desc.depthBiasClamp = webgpu_desc->depthBiasClamp();

  return dawn_desc;
}

}  // anonymous namespace

// static
GPURenderPipeline* GPURenderPipeline::Create(
    ScriptState* script_state,
    GPUDevice* device,
    const GPURenderPipelineDescriptor* webgpu_desc) {
  DCHECK(device);
  DCHECK(webgpu_desc);

  DawnRenderPipelineDescriptor dawn_desc;
  dawn_desc.nextInChain = nullptr;
  dawn_desc.layout = AsDawnType(webgpu_desc->layout());

  using PipelineStageInfo = std::tuple<DawnPipelineStageDescriptor, CString>;

  PipelineStageInfo vertex_stage_info = AsDawnType(webgpu_desc->vertexStage());
  dawn_desc.vertexStage = &std::get<0>(vertex_stage_info);

  // TODO(crbug.com/dawn/136): Support vertex-only pipelines.
  PipelineStageInfo fragment_stage_info =
      AsDawnType(webgpu_desc->fragmentStage());
  dawn_desc.fragmentStage = &std::get<0>(fragment_stage_info);

  // TODO(crbug.com/dawn/131): Update Dawn to match WebGPU vertex input
  v8::Isolate* isolate = script_state->GetIsolate();
  ExceptionState exception_state(isolate, ExceptionState::kConstructionContext,
                                 "GPUVertexInputDescriptor");
  DawnVertexInputInfo vertex_input_info = GPUVertexInputAsDawnInputState(
      isolate, webgpu_desc->vertexInput(), exception_state);
  dawn_desc.vertexInput = &std::get<0>(vertex_input_info);

  if (exception_state.HadException()) {
    return nullptr;
  }

  dawn_desc.primitiveTopology =
      AsDawnEnum<DawnPrimitiveTopology>(webgpu_desc->primitiveTopology());

  DawnRasterizationStateDescriptor rasterization_state =
      AsDawnType(webgpu_desc->rasterizationState());
  dawn_desc.rasterizationState = &rasterization_state;

  dawn_desc.sampleCount = webgpu_desc->sampleCount();

  DawnDepthStencilStateDescriptor depth_stencil_state;
  if (webgpu_desc->hasDepthStencilState()) {
    depth_stencil_state = AsDawnType(webgpu_desc->depthStencilState());
    dawn_desc.depthStencilState = &depth_stencil_state;
  } else {
    dawn_desc.depthStencilState = nullptr;
  }

  std::unique_ptr<DawnColorStateDescriptor[]> color_states =
      AsDawnType(webgpu_desc->colorStates());
  dawn_desc.colorStateCount =
      static_cast<uint32_t>(webgpu_desc->colorStates().size());

  DawnColorStateDescriptor* color_state_descriptors = color_states.get();
  dawn_desc.colorStates = &color_state_descriptors;

  return MakeGarbageCollected<GPURenderPipeline>(
      device, device->GetProcs().deviceCreateRenderPipeline(device->GetHandle(),
                                                            &dawn_desc));
}

GPURenderPipeline::GPURenderPipeline(GPUDevice* device,
                                     DawnRenderPipeline render_pipeline)
    : DawnObject<DawnRenderPipeline>(device, render_pipeline) {}

GPURenderPipeline::~GPURenderPipeline() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().renderPipelineRelease(GetHandle());
}

}  // namespace blink
