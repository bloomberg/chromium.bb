// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/supplementable.h"

// Forward declarations from webgpu.h
struct WGPUDeviceProperties;
typedef struct WGPUBufferImpl* WGPUBuffer;
// Forward declaration from dawn_proc.h
struct DawnProcTable;

namespace blink {

class GPUAdapter;
class GPUBuffer;
class GPURequestAdapterOptions;
class NavigatorBase;
class ScriptPromiseResolver;
class ScriptState;
class DawnControlClientHolder;

struct BoxedMappableWGPUBufferHandles
    : public RefCounted<BoxedMappableWGPUBufferHandles> {
 public:
  // Basic typed wrapper around |contents_|.
  void insert(WGPUBuffer buffer) { contents_.insert(buffer); }

  // Basic typed wrapper around |contents_|.
  void erase(WGPUBuffer buffer) { contents_.erase(buffer); }

  void ClearAndDestroyAll(const DawnProcTable& procs);

 private:
  // void* because HashSet tries to infer if T is GarbageCollected,
  // but WGPUBufferImpl has no real definition. We could define
  // IsGarbageCollectedType<struct WGPUBufferImpl> but it could easily
  // lead to a ODR violation.
  HashSet<void*> contents_;
};

class GPU final : public ScriptWrappable,
                  public Supplement<NavigatorBase>,
                  public ExecutionContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];

  // Getter for navigator.gpu
  static GPU* gpu(NavigatorBase&);

  explicit GPU(NavigatorBase&);

  GPU(const GPU&) = delete;
  GPU& operator=(const GPU&) = delete;

  ~GPU() override;

  // ScriptWrappable overrides
  void Trace(Visitor* visitor) const override;

  // ExecutionContextLifecycleObserver overrides
  void ContextDestroyed() override;

  // gpu.idl
  ScriptPromise requestAdapter(ScriptState* script_state,
                               const GPURequestAdapterOptions* options);

  // Store the buffer in a weak hash set so we can destroy it when the
  // context is destroyed.
  void TrackMappableBuffer(GPUBuffer* buffer);
  // Untrack the GPUBuffer. This is called eagerly when the buffer is
  // destroyed.
  void UntrackMappableBuffer(GPUBuffer* buffer);

  BoxedMappableWGPUBufferHandles* mappable_buffer_handles() const {
    return mappable_buffer_handles_.get();
  }

 private:
  void OnRequestAdapterCallback(ScriptState* script_state,
                                const GPURequestAdapterOptions* options,
                                ScriptPromiseResolver* resolver,
                                int32_t adapter_server_id,
                                const WGPUDeviceProperties& properties,
                                const char* error_message);

  void RecordAdapterForIdentifiability(ScriptState* script_state,
                                       const GPURequestAdapterOptions* options,
                                       GPUAdapter* adapter) const;

  scoped_refptr<DawnControlClientHolder> dawn_control_client_;
  HeapHashSet<WeakMember<GPUBuffer>> mappable_buffers_;
  // Mappable buffers remove themselves from this set on destruction.
  // It is boxed in a scoped_refptr so GPUBuffer can access it in its
  // destructor.
  scoped_refptr<BoxedMappableWGPUBufferHandles> mappable_buffer_handles_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_H_
