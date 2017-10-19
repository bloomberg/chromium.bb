// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTDisjointTimerQueryWebGL2_h
#define EXTDisjointTimerQueryWebGL2_h

#include "bindings/core/v8/ScriptValue.h"
#include "modules/webgl/WebGLExtension.h"
#include "modules/webgl/WebGLQuery.h"
#include "platform/wtf/HashMap.h"

namespace blink {

class WebGLRenderingContextBase;
class WebGLQuery;

class EXTDisjointTimerQueryWebGL2 final : public WebGLExtension {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static EXTDisjointTimerQueryWebGL2* Create(WebGLRenderingContextBase*);
  static bool Supported(WebGLRenderingContextBase*);
  static const char* ExtensionName();

  WebGLExtensionName GetName() const override;

  void queryCounterEXT(WebGLQuery*, GLenum);

  virtual void Trace(blink::Visitor*);

 private:
  explicit EXTDisjointTimerQueryWebGL2(WebGLRenderingContextBase*);
};

}  // namespace blink

#endif  // EXTDisjointTimerQueryWebGL2_h
