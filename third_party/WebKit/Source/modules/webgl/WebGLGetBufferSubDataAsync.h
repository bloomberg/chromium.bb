// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGLGetBufferSubDataAsync_h
#define WebGLGetBufferSubDataAsync_h

#include "bindings/core/v8/ScriptPromise.h"
#include "modules/webgl/WebGLExtension.h"

namespace blink {

class WebGL2RenderingContextBase;

class WebGLGetBufferSubDataAsync final : public WebGLExtension {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static WebGLGetBufferSubDataAsync* create(WebGLRenderingContextBase*);
  static bool supported(WebGLRenderingContextBase*);
  static const char* extensionName();

  WebGLExtensionName name() const override;

  ScriptPromise getBufferSubDataAsync(ScriptState*,
                                      GLenum target,
                                      GLintptr srcByteOffset,
                                      DOMArrayBufferView*,
                                      GLuint dstOffset,
                                      GLuint length);

 private:
  explicit WebGLGetBufferSubDataAsync(WebGLRenderingContextBase*);
};

class WebGLGetBufferSubDataAsyncCallback
    : public GarbageCollected<WebGLGetBufferSubDataAsyncCallback> {
 public:
  WebGLGetBufferSubDataAsyncCallback(WebGL2RenderingContextBase*,
                                     ScriptPromiseResolver*,
                                     void* shmReadbackResultData,
                                     GLuint commandsIssuedQueryID,
                                     DOMArrayBufferView*,
                                     void* destinationDataPtr,
                                     long long destinationByteLength);

  void destroy();

  void resolve();

  DECLARE_TRACE();

 private:
  WeakMember<WebGL2RenderingContextBase> m_context;
  Member<ScriptPromiseResolver> m_promiseResolver;

  // Pointer to shared memory where the gpu readback result is stored.
  void* m_shmReadbackResultData;
  // ID of the GL query used to call this callback.
  GLuint m_commandsIssuedQueryID;

  // ArrayBufferView returned from the promise.
  Member<DOMArrayBufferView> m_destinationArrayBufferView;
  // Pointer into the offset into destinationArrayBufferView.
  void* m_destinationDataPtr;
  // Size in bytes of the copy operation being performed.
  long long m_destinationByteLength;
};

}  // namespace blink

#endif  // WebGLGetBufferSubDataAsync_h
