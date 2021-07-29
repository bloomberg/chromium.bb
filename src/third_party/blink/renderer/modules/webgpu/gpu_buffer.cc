// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_buffer.h"

#include <cinttypes>
#include <utility>

#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "gpu/command_buffer/client/webgpu_interface.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_buffer_descriptor.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_callback.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_queue.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

// A size that if used to create a dawn_wire buffer, will guarantee we'll OOM
// immediately. It is an implementation detail of dawn_wire but that's tested
// on CQ in Dawn.
constexpr uint64_t kGuaranteedBufferOOMSize =
    std::numeric_limits<size_t>::max();

WGPUBufferDescriptor AsDawnType(const GPUBufferDescriptor* webgpu_desc,
                                std::string* label) {
  DCHECK(webgpu_desc);
  DCHECK(label);

  WGPUBufferDescriptor dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  dawn_desc.usage = AsDawnEnum<WGPUBufferUsage>(webgpu_desc->usage());
  dawn_desc.size = webgpu_desc->size();
  dawn_desc.mappedAtCreation = webgpu_desc->mappedAtCreation();
  if (webgpu_desc->hasLabel()) {
    *label = webgpu_desc->label().Utf8();
    dawn_desc.label = label->c_str();
  }

  return dawn_desc;
}

}  // namespace

// static
GPUBuffer* GPUBuffer::Create(GPUDevice* device,
                             const GPUBufferDescriptor* webgpu_desc) {
  DCHECK(device);

  std::string label;
  WGPUBufferDescriptor dawn_desc = AsDawnType(webgpu_desc, &label);

  // If the buffer is mappable, make sure the size stays in a size_t but still
  // guarantees that we have an OOM.
  bool is_mappable =
      dawn_desc.usage & (WGPUBufferUsage_MapRead | WGPUBufferUsage_MapWrite) ||
      dawn_desc.mappedAtCreation;
  if (is_mappable) {
    dawn_desc.size = std::min(dawn_desc.size, kGuaranteedBufferOOMSize);
  }

  GPUBuffer* buffer = MakeGarbageCollected<GPUBuffer>(
      device, dawn_desc.size,
      device->GetProcs().deviceCreateBuffer(device->GetHandle(), &dawn_desc));
  if (webgpu_desc->hasLabel())
    buffer->setLabel(webgpu_desc->label());
  return buffer;
}

GPUBuffer::GPUBuffer(GPUDevice* device,
                     uint64_t size,
                     WGPUBuffer buffer)
    : DawnObject<WGPUBuffer>(device, buffer), size_(size) {
}

void GPUBuffer::Trace(Visitor* visitor) const {
  visitor->Trace(mapped_array_buffers_);
  DawnObject<WGPUBuffer>::Trace(visitor);
}

ScriptPromise GPUBuffer::mapAsync(ScriptState* script_state,
                                  uint32_t mode,
                                  uint64_t offset,
                                  ExceptionState& exception_state) {
  return MapAsyncImpl(script_state, mode, offset, absl::nullopt,
                      exception_state);
}

ScriptPromise GPUBuffer::mapAsync(ScriptState* script_state,
                                  uint32_t mode,
                                  uint64_t offset,
                                  uint64_t size,
                                  ExceptionState& exception_state) {
  return MapAsyncImpl(script_state, mode, offset, size, exception_state);
}

DOMArrayBuffer* GPUBuffer::getMappedRange(ExecutionContext* execution_context,
                                          uint64_t offset,
                                          ExceptionState& exception_state) {
  return GetMappedRangeImpl(offset, absl::nullopt, execution_context,
                            exception_state);
}

DOMArrayBuffer* GPUBuffer::getMappedRange(ExecutionContext* execution_context,
                                          uint64_t offset,
                                          uint64_t size,
                                          ExceptionState& exception_state) {
  return GetMappedRangeImpl(offset, size, execution_context, exception_state);
}

void GPUBuffer::unmap(ScriptState* script_state) {
  ResetMappingState(script_state);
  GetProcs().bufferUnmap(GetHandle());
}

void GPUBuffer::destroy(ScriptState* script_state) {
  ResetMappingState(script_state);
  GetProcs().bufferDestroy(GetHandle());
}

ScriptPromise GPUBuffer::MapAsyncImpl(ScriptState* script_state,
                                      uint32_t mode,
                                      uint64_t offset,
                                      absl::optional<uint64_t> size,
                                      ExceptionState& exception_state) {
  // Compute the defaulted size which is "until the end of the buffer" or 0 if
  // offset is past the end of the buffer.
  uint64_t size_defaulted = 0;
  if (size) {
    size_defaulted = *size;
  } else if (offset <= size_) {
    size_defaulted = size_ - offset;
  }

  // We need to convert from uint64_t to size_t. Either of these two variables
  // are bigger or equal to the guaranteed OOM size then mapAsync should be an
  // error so. That OOM size fits in a size_t so we can clamp size and offset
  // with it.
  size_t map_offset =
      static_cast<size_t>(std::min(offset, kGuaranteedBufferOOMSize));
  size_t map_size =
      static_cast<size_t>(std::min(size_defaulted, kGuaranteedBufferOOMSize));

  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  // And send the command, leaving remaining validation to Dawn.
  auto* callback =
      BindDawnCallback(&GPUBuffer::OnMapAsyncCallback, WrapPersistent(this),
                       WrapPersistent(resolver));

  GetProcs().bufferMapAsync(GetHandle(), mode, map_offset, map_size,
                            callback->UnboundCallback(),
                            callback->AsUserdata());

  // WebGPU guarantees that promises are resolved in finite time so we
  // need to ensure commands are flushed.
  EnsureFlush();
  return promise;
}

DOMArrayBuffer* GPUBuffer::GetMappedRangeImpl(
    uint64_t offset,
    absl::optional<uint64_t> size,
    ExecutionContext* execution_context,
    ExceptionState& exception_state) {
  // Compute the defaulted size which is "until the end of the buffer" or 0 if
  // offset is past the end of the buffer.
  uint64_t size_defaulted = 0;
  if (size) {
    size_defaulted = *size;
  } else if (offset <= size_) {
    size_defaulted = size_ - offset;
  }

  // We need to convert from uint64_t to size_t. Either of these two variables
  // are bigger or equal to the guaranteed OOM size then getMappedRange should
  // be an error so. That OOM size fits in a size_t so we can clamp size and
  // offset with it.
  size_t range_offset =
      static_cast<size_t>(std::min(offset, kGuaranteedBufferOOMSize));
  size_t range_size =
      static_cast<size_t>(std::min(size_defaulted, kGuaranteedBufferOOMSize));

  // The maximum size that can be mapped in JS so that we can ensure we don't
  // create mappable buffers bigger than it.
  // This could eventually be upgrade to the max ArrayBuffer size instead of the
  // max TypedArray size. See crbug.com/951196
  if (range_size > v8::TypedArray::kMaxLength) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kOperationError,
        "getMappedRange failed, size is too large for the implementation");
    return nullptr;
  }

  if (range_size > std::numeric_limits<size_t>::max() - range_offset) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kOperationError,
        "getMappedRange failed, offset + size overflows size_t");
    return nullptr;
  }
  size_t range_end = range_offset + range_size;

  // Check if an overlapping range has already been returned.
  // TODO: keep mapped_ranges_ sorted (e.g. std::map), and do a binary search
  // (e.g. map.upper_bound()) to make this O(lg(n)) instead of linear.
  // (Note: std::map is not allowed in Blink.)
  for (const auto& overlap_candidate : mapped_ranges_) {
    size_t candidate_start = overlap_candidate.first;
    size_t candidate_end = overlap_candidate.second;
    if (range_end > candidate_start && range_offset < candidate_end) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kOperationError,
          WTF::String::Format("getMappedRange [%zu, %zu) overlaps with "
                              "previously returned range [%zu, %zu).",
                              range_offset, range_end, candidate_start,
                              candidate_end));
      return nullptr;
    }
  }

  // And send the command, leaving remaining validation to Dawn.
  const void* map_data_const = GetProcs().bufferGetConstMappedRange(
      GetHandle(), range_offset, range_size);

  if (!map_data_const) {
    // TODO: have explanatory error messages here (or just leave them to the
    // asynchronous error reporting).
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "getMappedRange failed");
    return nullptr;
  }

  // It is safe to const_cast the |data| pointer because it is a shadow
  // copy that Dawn wire makes and does not point to the mapped GPU
  // data. Dawn wire's copy of the data is not used outside of tests.
  uint8_t* map_data =
      const_cast<uint8_t*>(static_cast<const uint8_t*>(map_data_const));

  mapped_ranges_.push_back(std::make_pair(range_offset, range_end));
  return CreateArrayBufferForMappedData(map_data, range_size,
                                        execution_context);
}

void GPUBuffer::OnMapAsyncCallback(ScriptPromiseResolver* resolver,
                                   WGPUBufferMapAsyncStatus status) {
  switch (status) {
    case WGPUBufferMapAsyncStatus_Success:
      resolver->Resolve();
      break;
    case WGPUBufferMapAsyncStatus_Error:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kOperationError, "Could not mapAsync"));
      break;
    case WGPUBufferMapAsyncStatus_Unknown:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kOperationError, "Unknown error in mapAsync"));
      break;
    case WGPUBufferMapAsyncStatus_DeviceLost:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kAbortError, "Device is lost"));
      break;
    case WGPUBufferMapAsyncStatus_DestroyedBeforeCallback:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kAbortError,
          "Buffer is destroyed before the mapping is resolved"));
      break;
    case WGPUBufferMapAsyncStatus_UnmappedBeforeCallback:
      resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kAbortError,
          "Buffer is unmapped before the mapping is resolved"));
      break;
    default:
      NOTREACHED();
  }
}

DOMArrayBuffer* GPUBuffer::CreateArrayBufferForMappedData(
    void* data,
    size_t data_length,
    ExecutionContext* execution_context) {
  DCHECK(data);
  DCHECK_LE(static_cast<uint64_t>(data_length), v8::TypedArray::kMaxLength);

  // GPUBuffer::GetMappedRange returns ArrayBuffers that point to memory owned
  // by handle_, which is a dawn_wire::client::Buffer. It is possible that the
  // GPUBuffer gets garbage collected before the ArrayBuffer. When that happens
  // the dawn_wire::client::Buffer must be kept alive, otherwise the ArrayBuffer
  // will point to freed memory.
  //
  // To prevent this issue we make the ArrayBuffer keep a reference to the
  // WGPUBuffer, by referencing the buffer and then have a custom deleter for
  // the v8 backing store that releases that reference.
  //
  // V8 can call the ArrayBuffer deleter on any thread but dawn_wire and the
  // rest of the WebGPU implementation expect to be used on a single thread at
  // the moment. To fix this we keep a reference to the task runner for the
  // execution context that called getMappedRange and do a deletion on a task
  // posted to it.
  struct ArrayBufferStrongRefs {
    scoped_refptr<base::SingleThreadTaskRunner> task_runner;
    scoped_refptr<DawnControlClientHolder> dawn_control_client;
    WGPUBuffer dawn_buffer;
  };

  GetProcs().bufferReference(GetHandle());
  ArrayBufferStrongRefs* refs = new ArrayBufferStrongRefs{
      execution_context->GetTaskRunner(TaskType::kWebGPU),
      GetDawnControlClient(), GetHandle()};

  v8::BackingStore::DeleterCallback deleter = [](void*, size_t,
                                                 void* userdata) {
    ArrayBufferStrongRefs* refs = static_cast<ArrayBufferStrongRefs*>(userdata);

    // Happy case, we happen to be called on the correct thread. Release the
    // buffer immediately.
    if (refs->task_runner->BelongsToCurrentThread()) {
      refs->dawn_control_client->GetProcs().bufferRelease(refs->dawn_buffer);
      delete refs;
      return;
    }

    refs->task_runner->PostTask(
        FROM_HERE, ConvertToBaseOnceCallback(WTF::CrossThreadBindOnce(
                       [](ArrayBufferStrongRefs* refs) {
                         refs->dawn_control_client->GetProcs().bufferRelease(
                             refs->dawn_buffer);
                         delete refs;
                       },
                       WTF::CrossThreadUnretained(refs))));
  };

  ArrayBufferContents contents(
      v8::ArrayBuffer::NewBackingStore(data, data_length, deleter, refs));
  DOMArrayBuffer* array_buffer = DOMArrayBuffer::Create(contents);

  mapped_array_buffers_.push_back(array_buffer);
  return array_buffer;
}

void GPUBuffer::ResetMappingState(ScriptState* script_state) {
  mapped_ranges_.clear();

  for (Member<DOMArrayBuffer>& mapped_array_buffer : mapped_array_buffers_) {
    v8::Isolate* isolate = script_state->GetIsolate();
    DOMArrayBuffer* array_buffer = mapped_array_buffer.Release();
    DCHECK(array_buffer->IsDetachable(isolate));

    // Detach the array buffer by transferring the contents out and dropping
    // them.
    ArrayBufferContents contents;
    bool did_detach = array_buffer->Transfer(isolate, contents);

    // |did_detach| would be false if the buffer were already detached.
    DCHECK(did_detach);
    DCHECK(array_buffer->IsDetached());
  }
  mapped_array_buffers_.clear();
}

}  // namespace blink
