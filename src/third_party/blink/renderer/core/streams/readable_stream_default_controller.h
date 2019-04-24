// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_STREAM_DEFAULT_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_STREAM_DEFAULT_CONTROLLER_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_member.h"
#include "v8/include/v8.h"

namespace blink {

class ExceptionState;
class ScriptState;
class ScriptValue;

class ReadableStreamDefaultController : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  void close(ScriptState*);
  void enqueue(ScriptState*, ExceptionState&);
  void enqueue(ScriptState*, ScriptValue chunk, ExceptionState&);
  void error(ScriptState*);
  void error(ScriptState*, ScriptValue e);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_READABLE_STREAM_DEFAULT_CONTROLLER_H_
