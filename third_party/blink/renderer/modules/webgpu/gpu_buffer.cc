// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_buffer.h"

#include "gpu/command_buffer/client/webgpu_interface.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_callback.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_buffer_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

// crbug.com/951196
// Currently, this value is less than the maximum ArrayBuffer length which is
// theoretically 2^53 - 1 (Number.MAX_SAFE_INTEGER). However, creating a typed
// array from an ArrayBuffer of size greater than TypedArray::kMaxLength crashes
// DevTools and gives obscure errors.
constexpr size_t kLargestMappableSize = v8::TypedArray::kMaxLength;

bool ValidateMapSize(uint64_t buffer_size,
                     ScriptPromiseResolver* resolver,
                     ExceptionState& exception_state) {
  if (buffer_size > kLargestMappableSize) {
    WTF::StringBuilder message_builder;
    message_builder.Append(WTF::StringView("Buffer of "));
    message_builder.AppendNumber(buffer_size);
    message_builder.Append(WTF::StringView(" bytes too large for mapping"));

    WTF::String message = message_builder.ToString();

    exception_state.ThrowRangeError(message);
    resolver->Reject(
        DOMException::Create(DOMExceptionCode::kOperationError, message));

    return false;
  }
  return true;
}

}  // namespace

// static
GPUBuffer* GPUBuffer::Create(GPUDevice* device,
                             const GPUBufferDescriptor* webgpu_desc) {
  DCHECK(device);
  DCHECK(webgpu_desc);

  DawnBufferDescriptor dawn_desc;
  dawn_desc.nextInChain = nullptr;
  dawn_desc.usage = AsDawnEnum<DawnBufferUsageBit>(webgpu_desc->usage());
  dawn_desc.size = webgpu_desc->size();

  return MakeGarbageCollected<GPUBuffer>(
      device, dawn_desc.size,
      device->GetProcs().deviceCreateBuffer(device->GetHandle(), &dawn_desc));
}

GPUBuffer::GPUBuffer(GPUDevice* device, uint64_t size, DawnBuffer buffer)
    : DawnObject<DawnBuffer>(device, buffer), size_(size) {}

GPUBuffer::~GPUBuffer() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().bufferRelease(GetHandle());
}

void GPUBuffer::Trace(blink::Visitor* visitor) {
  visitor->Trace(mapped_buffer_);
  DawnObject<DawnBuffer>::Trace(visitor);
}

void GPUBuffer::setSubData(uint64_t dst_byte_offset,
                           const MaybeShared<DOMArrayBufferView>& src,
                           uint64_t src_byte_offset,
                           uint64_t byte_length,
                           ExceptionState& exception_state) {
  const uint8_t* src_base =
      reinterpret_cast<const uint8_t*>(src.View()->BaseAddress());
  size_t src_byte_length = src.View()->byteLength();

  if (src_byte_offset > src_byte_length) {
    exception_state.ThrowRangeError("srcOffset is too large");
    return;
  }

  if (byte_length == 0) {
    byte_length = src_byte_length - src_byte_offset;
  } else if (byte_length > src_byte_length - src_byte_offset) {
    exception_state.ThrowRangeError("byteLength is too large");
    return;
  }

  const uint8_t* data = &src_base[src_byte_offset];
  GetProcs().bufferSetSubData(GetHandle(), dst_byte_offset, byte_length, data);
}

void GPUBuffer::OnMapAsyncCallback(ScriptPromiseResolver* resolver,
                                   DawnBufferMapAsyncStatus status,
                                   void* data,
                                   uint64_t data_length) {
  switch (status) {
    case DAWN_BUFFER_MAP_ASYNC_STATUS_SUCCESS:
      DCHECK(data);
      DCHECK_LE(data_length, kLargestMappableSize);
      {
        WTF::ArrayBufferContents::DataHandle handle(
            data, static_cast<size_t>(data_length),
            [](void* data, size_t length, void* info) {
              // DataDeleter does nothing because Dawn wire owns the memory.
            },
            nullptr);

        WTF::ArrayBufferContents contents(
            std::move(handle),
            WTF::ArrayBufferContents::SharingType::kNotShared);

        mapped_buffer_ = DOMArrayBuffer::Create(contents);
        resolver->Resolve(mapped_buffer_);
      }
      break;
    case DAWN_BUFFER_MAP_ASYNC_STATUS_ERROR:
      resolver->Reject(DOMException::Create(DOMExceptionCode::kOperationError));
      break;
    case DAWN_BUFFER_MAP_ASYNC_STATUS_UNKNOWN:
    case DAWN_BUFFER_MAP_ASYNC_STATUS_CONTEXT_LOST:
      resolver->Reject(DOMException::Create(DOMExceptionCode::kAbortError));
      break;
    default:
      NOTREACHED();
      resolver->Reject(DOMException::Create(DOMExceptionCode::kAbortError));
      break;
  }
}

ScriptPromise GPUBuffer::mapReadAsync(ScriptState* script_state,
                                      ExceptionState& exception_state) {
  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!ValidateMapSize(size_, resolver, exception_state)) {
    return promise;
  }

  auto* callback =
      BindDawnCallback(&GPUBuffer::OnMapAsyncCallback, WrapPersistent(this),
                       WrapPersistent(resolver));

  using Callback = std::remove_reference_t<decltype(*callback)>;

  GetProcs().bufferMapReadAsync(
      GetHandle(),
      [](DawnBufferMapAsyncStatus status, const void* data,
         uint64_t data_length, DawnCallbackUserdata userdata) {
        // It is safe to const_cast the |data| pointer because it is a shadow
        // copy that Dawn wire makes and does not point to the mapped GPU data.
        // Dawn wire's copy of the data is not used outside of tests.
        return Callback::CallUnboundCallback(status, const_cast<void*>(data),
                                             data_length, userdata);
      },
      callback->AsUserdata());
  // WebGPU guarantees callbacks complete in finite time. Flush now so that
  // commands reach the GPU process.
  device_->GetInterface()->FlushCommands();

  return promise;
}

ScriptPromise GPUBuffer::mapWriteAsync(ScriptState* script_state,
                                       ExceptionState& exception_state) {
  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!ValidateMapSize(size_, resolver, exception_state)) {
    return promise;
  }

  auto* callback =
      BindDawnCallback(&GPUBuffer::OnMapAsyncCallback, WrapPersistent(this),
                       WrapPersistent(resolver));

  GetProcs().bufferMapWriteAsync(GetHandle(), callback->UnboundCallback(),
                                 callback->AsUserdata());
  // WebGPU guarantees callbacks complete in finite time. Flush now so that
  // commands reach the GPU process.
  device_->GetInterface()->FlushCommands();

  return promise;
}

void GPUBuffer::unmap(ScriptState* script_state) {
  DetachArrayBufferForCurrentMapping(script_state);
  GetProcs().bufferUnmap(GetHandle());
}

void GPUBuffer::destroy(ScriptState* script_state) {
  DetachArrayBufferForCurrentMapping(script_state);
  GetProcs().bufferDestroy(GetHandle());
}

void GPUBuffer::DetachArrayBufferForCurrentMapping(ScriptState* script_state) {
  if (!mapped_buffer_) {
    return;
  }
  v8::Isolate* isolate = script_state->GetIsolate();
  DOMArrayBuffer* mapped_buffer = mapped_buffer_.Release();
  DCHECK(mapped_buffer->IsNeuterable(isolate));

  // Detach the array buffer by transferring the contents out and dropping them.
  WTF::ArrayBufferContents contents;
  DCHECK(mapped_buffer->Transfer(isolate, contents));
}

}  // namespace blink
