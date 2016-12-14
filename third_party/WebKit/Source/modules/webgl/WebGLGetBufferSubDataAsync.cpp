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

WebGLExtensionName WebGLGetBufferSubDataAsync::name() const {
  return WebGLGetBufferSubDataAsyncName;
}

WebGLGetBufferSubDataAsync* WebGLGetBufferSubDataAsync::create(
    WebGLRenderingContextBase* context) {
  return new WebGLGetBufferSubDataAsync(context);
}

bool WebGLGetBufferSubDataAsync::supported(WebGLRenderingContextBase* context) {
  return true;
}

const char* WebGLGetBufferSubDataAsync::extensionName() {
  return "WEBGL_get_buffer_sub_data_async";
}

ScriptPromise WebGLGetBufferSubDataAsync::getBufferSubDataAsync(
    ScriptState* scriptState,
    GLenum target,
    GLintptr srcByteOffset,
    DOMArrayBufferView* dstData,
    GLuint dstOffset,
    GLuint length) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  WebGLExtensionScopedContext scoped(this);
  if (scoped.isLost()) {
    DOMException* exception =
        DOMException::create(InvalidStateError, "context lost");
    resolver->reject(exception);
    return promise;
  }

  WebGL2RenderingContextBase* context = nullptr;
  {
    WebGLRenderingContextBase* contextBase = scoped.context();
    DCHECK_GE(contextBase->version(), 2u);
    context = static_cast<WebGL2RenderingContextBase*>(contextBase);
  }

  WebGLBuffer* sourceBuffer = nullptr;
  void* destinationDataPtr = nullptr;
  long long destinationByteLength = 0;
  const char* message = context->validateGetBufferSubData(
      __FUNCTION__, target, srcByteOffset, dstData, dstOffset, length,
      &sourceBuffer, &destinationDataPtr, &destinationByteLength);
  if (message) {
    // If there was a GL error, it was already synthesized in
    // validateGetBufferSubData, so it's not done here.
    DOMException* exception = DOMException::create(InvalidStateError, message);
    resolver->reject(exception);
    return promise;
  }

  message = context->validateGetBufferSubDataBounds(
      __FUNCTION__, sourceBuffer, srcByteOffset, destinationByteLength);
  if (message) {
    // If there was a GL error, it was already synthesized in
    // validateGetBufferSubDataBounds, so it's not done here.
    DOMException* exception = DOMException::create(InvalidStateError, message);
    resolver->reject(exception);
    return promise;
  }

  // If the length of the copy is zero, this is a no-op.
  if (!destinationByteLength) {
    resolver->resolve(dstData);
    return promise;
  }

  GLuint queryID;
  context->contextGL()->GenQueriesEXT(1, &queryID);
  context->contextGL()->BeginQueryEXT(GL_COMMANDS_ISSUED_CHROMIUM, queryID);
  void* mappedData = context->contextGL()->GetBufferSubDataAsyncCHROMIUM(
      target, srcByteOffset, destinationByteLength);
  context->contextGL()->EndQueryEXT(GL_COMMANDS_ISSUED_CHROMIUM);
  if (!mappedData) {
    DOMException* exception =
        DOMException::create(InvalidStateError, "Out of memory");
    resolver->reject(exception);
    return promise;
  }

  auto callbackObject = new WebGLGetBufferSubDataAsyncCallback(
      context, resolver, mappedData, queryID, dstData, destinationDataPtr,
      destinationByteLength);
  context->registerGetBufferSubDataAsyncCallback(callbackObject);
  auto callback = WTF::bind(&WebGLGetBufferSubDataAsyncCallback::resolve,
                            wrapPersistent(callbackObject));
  context->drawingBuffer()->contextProvider()->signalQuery(
      queryID, convertToBaseCallback(std::move(callback)));

  return promise;
}

WebGLGetBufferSubDataAsyncCallback::WebGLGetBufferSubDataAsyncCallback(
    WebGL2RenderingContextBase* context,
    ScriptPromiseResolver* promiseResolver,
    void* shmReadbackResultData,
    GLuint commandsIssuedQueryID,
    DOMArrayBufferView* destinationArrayBufferView,
    void* destinationDataPtr,
    long long destinationByteLength)
    : m_context(context),
      m_promiseResolver(promiseResolver),
      m_shmReadbackResultData(shmReadbackResultData),
      m_commandsIssuedQueryID(commandsIssuedQueryID),
      m_destinationArrayBufferView(destinationArrayBufferView),
      m_destinationDataPtr(destinationDataPtr),
      m_destinationByteLength(destinationByteLength) {
  DCHECK(shmReadbackResultData);
  DCHECK(destinationDataPtr);
}

void WebGLGetBufferSubDataAsyncCallback::destroy() {
  DCHECK(m_shmReadbackResultData);
  m_context->contextGL()->FreeSharedMemory(m_shmReadbackResultData);
  m_shmReadbackResultData = nullptr;
  DOMException* exception =
      DOMException::create(InvalidStateError, "Context lost or destroyed");
  m_promiseResolver->reject(exception);
}

void WebGLGetBufferSubDataAsyncCallback::resolve() {
  if (!m_context || !m_shmReadbackResultData) {
    DOMException* exception =
        DOMException::create(InvalidStateError, "Context lost or destroyed");
    m_promiseResolver->reject(exception);
    return;
  }
  if (m_destinationArrayBufferView->buffer()->isNeutered()) {
    DOMException* exception = DOMException::create(
        InvalidStateError, "ArrayBufferView became invalid asynchronously");
    m_promiseResolver->reject(exception);
    return;
  }
  memcpy(m_destinationDataPtr, m_shmReadbackResultData,
         m_destinationByteLength);
  // TODO(kainino): What would happen if the DOM was suspended when the
  // promise became resolved? Could another JS task happen between the memcpy
  // and the promise resolution task, which would see the wrong data?
  m_promiseResolver->resolve(m_destinationArrayBufferView);

  m_context->contextGL()->DeleteQueriesEXT(1, &m_commandsIssuedQueryID);
  this->destroy();
  m_context->unregisterGetBufferSubDataAsyncCallback(this);
}

DEFINE_TRACE(WebGLGetBufferSubDataAsyncCallback) {
  visitor->trace(m_context);
  visitor->trace(m_promiseResolver);
  visitor->trace(m_destinationArrayBufferView);
}

}  // namespace blink
