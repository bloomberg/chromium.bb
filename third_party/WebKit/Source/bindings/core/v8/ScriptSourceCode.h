/*
 * Copyright (C) 2008, 2009 Google Inc. All rights reserved.
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

#ifndef ScriptSourceCode_h
#define ScriptSourceCode_h

#include "bindings/core/v8/ScriptStreamer.h"
#include "core/CoreExport.h"
#include "core/loader/resource/ScriptResource.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/text/TextPosition.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class CORE_EXPORT ScriptSourceCode final {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  ScriptSourceCode();
  // We lose the encoding information from ScriptResource.
  // Not sure if that matters.
  explicit ScriptSourceCode(ScriptResource*);
  ScriptSourceCode(
      const String& source,
      const KURL& = KURL(),
      const TextPosition& start_position = TextPosition::MinimumPosition());
  ScriptSourceCode(ScriptStreamer*, ScriptResource*);

  ~ScriptSourceCode();
  DECLARE_TRACE();

  // The null value represents a missing script, created by the nullary
  // constructor, and differs from the empty script.
  bool IsNull() const { return source_.IsNull(); }

  const String& Source() const { return source_; }
  ScriptResource* GetResource() const { return resource_; }
  const KURL& Url() const;
  int StartLine() const { return start_position_.line_.OneBasedInt(); }
  const TextPosition& StartPosition() const { return start_position_; }
  String SourceMapUrl() const;

  ScriptStreamer* Streamer() const { return streamer_; }

 private:
  void TreatNullSourceAsEmpty();

  String source_;
  Member<ScriptResource> resource_;
  Member<ScriptStreamer> streamer_;
  mutable KURL url_;
  TextPosition start_position_;
};

}  // namespace blink

WTF_ALLOW_INIT_WITH_MEM_FUNCTIONS(blink::ScriptSourceCode);

#endif  // ScriptSourceCode_h
