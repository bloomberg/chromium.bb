// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webgl/WebGLGetBufferSubDataAsync.h"

#include "core/dom/DOMException.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "modules/webgl/WebGL2RenderingContextBase.h"

namespace blink {

WebGLGetBufferSubDataAsync::WebGLGetBufferSubDataAsync(
    WebGLRenderingContextBase* context)
    : WebGLExtension(context) {}

WebGLExtensionName WebGLGetBufferSubDataAsync::GetName() const {
  return kWebGLGetBufferSubDataAsyncName;
}

WebGLGetBufferSubDataAsync* WebGLGetBufferSubDataAsync::Create(
    WebGLRenderingContextBase* context) {
  return new WebGLGetBufferSubDataAsync(context);
}

bool WebGLGetBufferSubDataAsync::Supported(WebGLRenderingContextBase* context) {
  return true;
}

const char* WebGLGetBufferSubDataAsync::ExtensionName() {
  return "WEBGL_get_buffer_sub_data_async";
}

ScriptPromise WebGLGetBufferSubDataAsync::getBufferSubDataAsync(
    ScriptState* script_state,
    GLenum target,
    GLintptr src_byte_offset,
    NotShared<DOMArrayBufferView> dst_data,
    GLuint dst_offset,
    GLuint length) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  WebGLExtensionScopedContext scoped(this);
  if (scoped.IsLost()) {
    DOMException* exception =
        DOMException::Create(kInvalidStateError, "context lost");
    resolver->Reject(exception);
    return promise;
  }

  WebGL2RenderingContextBase* context = nullptr;
  {
    WebGLRenderingContextBase* context_base = scoped.Context();
    DCHECK_GE(context_base->Version(), 2u);
    context = static_cast<WebGL2RenderingContextBase*>(context_base);
  }

  WebGLBuffer* source_buffer = nullptr;
  void* destination_data_ptr = nullptr;
  long long destination_byte_length = 0;
  const char* message = context->ValidateGetBufferSubData(
      __FUNCTION__, target, src_byte_offset, dst_data.View(), dst_offset,
      length, &source_buffer, &destination_data_ptr, &destination_byte_length);
  if (message) {
    // If there was a GL error, it was already synthesized in
    // validateGetBufferSubData, so it's not done here.
    DOMException* exception = DOMException::Create(kInvalidStateError, message);
    resolver->Reject(exception);
    return promise;
  }

  message = context->ValidateGetBufferSubDataBounds(
      __FUNCTION__, source_buffer, src_byte_offset, destination_byte_length);
  if (message) {
    // If there was a GL error, it was already synthesized in
    // validateGetBufferSubDataBounds, so it's not done here.
    DOMException* exception = DOMException::Create(kInvalidStateError, message);
    resolver->Reject(exception);
    return promise;
  }

  // If the length of the copy is zero, this is a no-op.
  if (!destination_byte_length) {
    resolver->Resolve(dst_data.View());
    return promise;
  }

  GLuint query_id;
  context->ContextGL()->GenQueriesEXT(1, &query_id);
  context->ContextGL()->BeginQueryEXT(GL_COMMANDS_ISSUED_CHROMIUM, query_id);
  void* mapped_data = context->ContextGL()->GetBufferSubDataAsyncCHROMIUM(
      target, src_byte_offset, destination_byte_length);
  context->ContextGL()->EndQueryEXT(GL_COMMANDS_ISSUED_CHROMIUM);
  if (!mapped_data) {
    DOMException* exception =
        DOMException::Create(kInvalidStateError, "Out of memory");
    resolver->Reject(exception);
    return promise;
  }

  auto callback_object = new WebGLGetBufferSubDataAsyncCallback(
      context, resolver, mapped_data, query_id, dst_data.View(),
      destination_data_ptr, destination_byte_length);
  context->RegisterGetBufferSubDataAsyncCallback(callback_object);
  auto callback = WTF::Bind(&WebGLGetBufferSubDataAsyncCallback::Resolve,
                            WrapPersistent(callback_object));
  context->GetDrawingBuffer()->ContextProvider()->SignalQuery(
      query_id, ConvertToBaseCallback(std::move(callback)));

  return promise;
}

WebGLGetBufferSubDataAsyncCallback::WebGLGetBufferSubDataAsyncCallback(
    WebGL2RenderingContextBase* context,
    ScriptPromiseResolver* promise_resolver,
    void* shm_readback_result_data,
    GLuint commands_issued_query_id,
    DOMArrayBufferView* destination_array_buffer_view,
    void* destination_data_ptr,
    long long destination_byte_length)
    : context_(context),
      promise_resolver_(promise_resolver),
      shm_readback_result_data_(shm_readback_result_data),
      commands_issued_query_id_(commands_issued_query_id),
      destination_array_buffer_view_(destination_array_buffer_view),
      destination_data_ptr_(destination_data_ptr),
      destination_byte_length_(destination_byte_length) {
  DCHECK(shm_readback_result_data);
  DCHECK(destination_data_ptr);
}

void WebGLGetBufferSubDataAsyncCallback::Destroy() {
  DCHECK(shm_readback_result_data_);
  context_->ContextGL()->FreeSharedMemory(shm_readback_result_data_);
  shm_readback_result_data_ = nullptr;
  DOMException* exception =
      DOMException::Create(kInvalidStateError, "Context lost or destroyed");
  promise_resolver_->Reject(exception);
}

void WebGLGetBufferSubDataAsyncCallback::Resolve() {
  if (!context_ || !shm_readback_result_data_) {
    DOMException* exception =
        DOMException::Create(kInvalidStateError, "Context lost or destroyed");
    promise_resolver_->Reject(exception);
    return;
  }
  if (destination_array_buffer_view_->buffer()->IsNeutered()) {
    DOMException* exception = DOMException::Create(
        kInvalidStateError, "ArrayBufferView became invalid asynchronously");
    promise_resolver_->Reject(exception);
    return;
  }
  memcpy(destination_data_ptr_, shm_readback_result_data_,
         destination_byte_length_);
  // TODO(kainino): What would happen if the DOM was suspended when the
  // promise became resolved? Could another JS task happen between the memcpy
  // and the promise resolution task, which would see the wrong data?
  promise_resolver_->Resolve(destination_array_buffer_view_);

  context_->ContextGL()->DeleteQueriesEXT(1, &commands_issued_query_id_);
  this->Destroy();
  context_->UnregisterGetBufferSubDataAsyncCallback(this);
}

void WebGLGetBufferSubDataAsyncCallback::Trace(blink::Visitor* visitor) {
  visitor->Trace(context_);
  visitor->Trace(promise_resolver_);
  visitor->Trace(destination_array_buffer_view_);
}

}  // namespace blink
