/*
 * Copyright (C) 2010 Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FileReaderSync_h
#define FileReaderSync_h

#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class Blob;
class DOMArrayBuffer;
class ExceptionState;
class ExecutionContext;
class FileReaderLoader;
class ScriptState;

class FileReaderSync final : public GarbageCollected<FileReaderSync>,
                             public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static FileReaderSync* Create(ExecutionContext* context) {
    return new FileReaderSync(context);
  }

  DOMArrayBuffer* readAsArrayBuffer(ScriptState*, Blob*, ExceptionState&);
  String readAsBinaryString(ScriptState*, Blob*, ExceptionState&);
  String readAsText(ScriptState* script_state, Blob* blob, ExceptionState& ec) {
    return readAsText(script_state, blob, "", ec);
  }
  String readAsText(ScriptState*,
                    Blob*,
                    const String& encoding,
                    ExceptionState&);
  String readAsDataURL(ScriptState*, Blob*, ExceptionState&);

  void Trace(blink::Visitor* visitor) {}

 private:
  explicit FileReaderSync(ExecutionContext*);

  void StartLoading(ExecutionContext*,
                    FileReaderLoader&,
                    const Blob&,
                    ExceptionState&);
};

}  // namespace blink

#endif  // FileReaderSync_h
