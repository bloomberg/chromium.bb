// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_GET_BUFFER_SUB_DATA_ASYNC_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_GET_BUFFER_SUB_DATA_ASYNC_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/modules/webgl/webgl_extension.h"

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

  void Trace(blink::Visitor*);

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

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_GET_BUFFER_SUB_DATA_ASYNC_H_
