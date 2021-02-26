//* Copyright 2017 The Dawn Authors
//*
//* Licensed under the Apache License, Version 2.0 (the "License");
//* you may not use this file except in compliance with the License.
//* You may obtain a copy of the License at
//*
//*     http://www.apache.org/licenses/LICENSE-2.0
//*
//* Unless required by applicable law or agreed to in writing, software
//* distributed under the License is distributed on an "AS IS" BASIS,
//* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//* See the License for the specific language governing permissions and
//* limitations under the License.

#include "mock_webgpu.h"

using namespace testing;

namespace {
    {% for type in by_category["object"] %}
        {% for method in c_methods(type) if len(method.arguments) < 10 %}
            {{as_cType(method.return_type.name)}} Forward{{as_MethodSuffix(type.name, method.name)}}(
                {{-as_cType(type.name)}} self
                {%- for arg in method.arguments -%}
                    , {{as_annotated_cType(arg)}}
                {%- endfor -%}
            ) {
                auto object = reinterpret_cast<ProcTableAsClass::Object*>(self);
                return object->procs->{{as_MethodSuffix(type.name, method.name)}}(self
                    {%- for arg in method.arguments -%}
                        , {{as_varName(arg.name)}}
                    {%- endfor -%}
                );
            }
        {% endfor %}

    {% endfor %}
}

ProcTableAsClass::~ProcTableAsClass() {
}

void ProcTableAsClass::GetProcTableAndDevice(DawnProcTable* table, WGPUDevice* device) {
    *device = GetNewDevice();

    {% for type in by_category["object"] %}
        {% for method in c_methods(type) if len(method.arguments) < 10 %}
            table->{{as_varName(type.name, method.name)}} = reinterpret_cast<{{as_cProc(type.name, method.name)}}>(Forward{{as_MethodSuffix(type.name, method.name)}});
        {% endfor %}
    {% endfor %}
}

void ProcTableAsClass::DeviceSetUncapturedErrorCallback(WGPUDevice self,
                                                        WGPUErrorCallback callback,
                                                        void* userdata) {
    auto object = reinterpret_cast<ProcTableAsClass::Object*>(self);
    object->deviceErrorCallback = callback;
    object->userdata = userdata;

    OnDeviceSetUncapturedErrorCallback(self, callback, userdata);
}

void ProcTableAsClass::DeviceSetDeviceLostCallback(WGPUDevice self,
                                                   WGPUDeviceLostCallback callback,
                                                   void* userdata) {
    auto object = reinterpret_cast<ProcTableAsClass::Object*>(self);
    object->deviceLostCallback = callback;
    object->userdata = userdata;

    OnDeviceSetDeviceLostCallback(self, callback, userdata);
}

bool ProcTableAsClass::DevicePopErrorScope(WGPUDevice self,
                                           WGPUErrorCallback callback,
                                           void* userdata) {
    return OnDevicePopErrorScopeCallback(self, callback, userdata);
}

void ProcTableAsClass::BufferMapAsync(WGPUBuffer self,
                                      WGPUMapModeFlags mode,
                                      size_t offset,
                                      size_t size,
                                      WGPUBufferMapCallback callback,
                                      void* userdata) {
    auto object = reinterpret_cast<ProcTableAsClass::Object*>(self);
    object->mapAsyncCallback = callback;
    object->userdata = userdata;

    OnBufferMapAsyncCallback(self, callback, userdata);
}

void ProcTableAsClass::FenceOnCompletion(WGPUFence self,
                                         uint64_t value,
                                         WGPUFenceOnCompletionCallback callback,
                                         void* userdata) {
    auto object = reinterpret_cast<ProcTableAsClass::Object*>(self);
    object->fenceOnCompletionCallback = callback;
    object->userdata = userdata;

    OnFenceOnCompletionCallback(self, value, callback, userdata);
}

void ProcTableAsClass::DeviceCreateReadyComputePipeline(
    WGPUDevice self,
    WGPUComputePipelineDescriptor const * descriptor,
    WGPUCreateReadyComputePipelineCallback callback,
    void* userdata) {
    auto object = reinterpret_cast<ProcTableAsClass::Object*>(self);
    object->createReadyComputePipelineCallback = callback;
    object->userdata = userdata;

    OnDeviceCreateReadyComputePipelineCallback(self, descriptor, callback, userdata);
}

void ProcTableAsClass::DeviceCreateReadyRenderPipeline(
    WGPUDevice self,
    WGPURenderPipelineDescriptor const * descriptor,
    WGPUCreateReadyRenderPipelineCallback callback,
    void* userdata) {
    auto object = reinterpret_cast<ProcTableAsClass::Object*>(self);
    object->createReadyRenderPipelineCallback = callback;
    object->userdata = userdata;

    OnDeviceCreateReadyRenderPipelineCallback(self, descriptor, callback, userdata);
}

void ProcTableAsClass::CallDeviceErrorCallback(WGPUDevice device,
                                               WGPUErrorType type,
                                               const char* message) {
    auto object = reinterpret_cast<ProcTableAsClass::Object*>(device);
    object->deviceErrorCallback(type, message, object->userdata);
}

void ProcTableAsClass::CallDeviceLostCallback(WGPUDevice device, const char* message) {
    auto object = reinterpret_cast<ProcTableAsClass::Object*>(device);
    object->deviceLostCallback(message, object->userdata);
}

void ProcTableAsClass::CallMapAsyncCallback(WGPUBuffer buffer, WGPUBufferMapAsyncStatus status) {
    auto object = reinterpret_cast<ProcTableAsClass::Object*>(buffer);
    object->mapAsyncCallback(status, object->userdata);
}

void ProcTableAsClass::CallFenceOnCompletionCallback(WGPUFence fence,
                                                     WGPUFenceCompletionStatus status) {
    auto object = reinterpret_cast<ProcTableAsClass::Object*>(fence);
    object->fenceOnCompletionCallback(status, object->userdata);
}

void ProcTableAsClass::CallDeviceCreateReadyComputePipelineCallback(WGPUDevice device,
                                                                    WGPUCreateReadyPipelineStatus status,
                                                                    WGPUComputePipeline pipeline,
                                                                    const char* message) {
    auto object = reinterpret_cast<ProcTableAsClass::Object*>(device);
    object->createReadyComputePipelineCallback(status, pipeline, message, object->userdata);
}

void ProcTableAsClass::CallDeviceCreateReadyRenderPipelineCallback(WGPUDevice device,
                                                                   WGPUCreateReadyPipelineStatus status,
                                                                   WGPURenderPipeline pipeline,
                                                                   const char* message) {
    auto object = reinterpret_cast<ProcTableAsClass::Object*>(device);
    object->createReadyRenderPipelineCallback(status, pipeline, message, object->userdata);
}

{% for type in by_category["object"] %}
    {{as_cType(type.name)}} ProcTableAsClass::GetNew{{type.name.CamelCase()}}() {
        mObjects.emplace_back(new Object);
        mObjects.back()->procs = this;
        return reinterpret_cast<{{as_cType(type.name)}}>(mObjects.back().get());
    }
{% endfor %}

MockProcTable::MockProcTable() = default;

MockProcTable::~MockProcTable() = default;

void MockProcTable::IgnoreAllReleaseCalls() {
    {% for type in by_category["object"] %}
        EXPECT_CALL(*this, {{as_MethodSuffix(type.name, Name("release"))}}(_)).Times(AnyNumber());
    {% endfor %}
}
