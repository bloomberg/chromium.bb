// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTDisjointTimerQuery_h
#define EXTDisjointTimerQuery_h

#include "bindings/core/v8/ScriptValue.h"
#include "modules/webgl/WebGLExtension.h"
#include "platform/wtf/HashMap.h"

namespace blink {

class WebGLRenderingContextBase;
class WebGLTimerQueryEXT;

class EXTDisjointTimerQuery final : public WebGLExtension {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static EXTDisjointTimerQuery* Create(WebGLRenderingContextBase*);
  static bool Supported(WebGLRenderingContextBase*);
  static const char* ExtensionName();

  WebGLExtensionName GetName() const override;

  WebGLTimerQueryEXT* createQueryEXT();
  void deleteQueryEXT(WebGLTimerQueryEXT*);
  GLboolean isQueryEXT(WebGLTimerQueryEXT*);
  void beginQueryEXT(GLenum, WebGLTimerQueryEXT*);
  void endQueryEXT(GLenum);
  void queryCounterEXT(WebGLTimerQueryEXT*, GLenum);
  ScriptValue getQueryEXT(ScriptState*, GLenum, GLenum);
  ScriptValue getQueryObjectEXT(ScriptState*, WebGLTimerQueryEXT*, GLenum);

  virtual void Trace(blink::Visitor*);

 private:
  friend class WebGLTimerQueryEXT;
  explicit EXTDisjointTimerQuery(WebGLRenderingContextBase*);

  Member<WebGLTimerQueryEXT> current_elapsed_query_;
};

}  // namespace blink

#endif  // EXTDisjointTimerQuery_h
