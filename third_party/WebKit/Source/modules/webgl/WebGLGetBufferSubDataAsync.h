// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGLGetBufferSubDataAsync_h
#define WebGLGetBufferSubDataAsync_h

#include "bindings/core/v8/ScriptPromise.h"
#include "core/typed_arrays/ArrayBufferViewHelpers.h"
#include "modules/webgl/WebGLExtension.h"

namespace blink {

class WebGL2RenderingContextBase;

class WebGLGetBufferSubDataAsync final : public WebGLExtension {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static WebGLGetBufferSubDataAsync* Create(WebGLRenderingContextBase*);
  static bool Supported(WebGLRenderingContextBase*);
  static const char* ExtensionName();

  WebGLExtensionName GetName() const override;

  ScriptPromise getBufferSubDataAsync(ScriptState*,
                                      GLenum target,
                                      GLintptr src_byte_offset,
                                      NotShared<DOMArrayBufferView>,
                                      GLuint dst_offset,
                                      GLuint length);

 private:
  explicit WebGLGetBufferSubDataAsync(WebGLRenderingContextBase*);
};

class WebGLGetBufferSubDataAsyncCallback
    : public GarbageCollected<WebGLGetBufferSubDataAsyncCallback> {
 public:
  WebGLGetBufferSubDataAsyncCallback(WebGL2RenderingContextBase*,
                                     ScriptPromiseResolver*,
                                     void* shm_readback_result_data,
                                     GLuint commands_issued_query_id,
                                     DOMArrayBufferView*,
                                     void* destination_data_ptr,
                                     long long destination_byte_length);

  void Destroy();

  void Resolve();

  DECLARE_TRACE();

 private:
  WeakMember<WebGL2RenderingContextBase> context_;
  Member<ScriptPromiseResolver> promise_resolver_;

  // Pointer to shared memory where the gpu readback result is stored.
  void* shm_readback_result_data_;
  // ID of the GL query used to call this callback.
  GLuint commands_issued_query_id_;

  // ArrayBufferView returned from the promise.
  Member<DOMArrayBufferView> destination_array_buffer_view_;
  // Pointer into the offset into destinationArrayBufferView.
  void* destination_data_ptr_;
  // Size in bytes of the copy operation being performed.
  long long destination_byte_length_;
};

}  // namespace blink

#endif  // WebGLGetBufferSubDataAsync_h
