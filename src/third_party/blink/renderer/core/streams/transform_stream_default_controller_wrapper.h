// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_TRANSFORM_STREAM_DEFAULT_CONTROLLER_WRAPPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_TRANSFORM_STREAM_DEFAULT_CONTROLLER_WRAPPER_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/streams/transform_stream_default_controller_interface.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "v8/include/v8.h"

namespace blink {

// Implementation of TransformStreamDefaultControllerInterface for the V8 Extras
// implementation of Streams.
class CORE_EXPORT TransformStreamDefaultControllerWrapper final
    : public TransformStreamDefaultControllerInterface {
  STACK_ALLOCATED();

 public:
  TransformStreamDefaultControllerWrapper(ScriptState*,
                                          v8::Local<v8::Value> controller);

  void Enqueue(v8::Local<v8::Value> chunk, ExceptionState&) override;

 private:
  Member<ScriptState> script_state_;
  v8::Local<v8::Value> controller_;

  DISALLOW_COPY_AND_ASSIGN(TransformStreamDefaultControllerWrapper);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_TRANSFORM_STREAM_DEFAULT_CONTROLLER_WRAPPER_H_
