/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef TextDecoder_h
#define TextDecoder_h

#include <memory>
#include "bindings/core/v8/ArrayBufferOrArrayBufferView.h"
#include "modules/encoding/TextDecodeOptions.h"
#include "modules/encoding/TextDecoderOptions.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/TextCodec.h"
#include "platform/wtf/text/TextEncoding.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class ExceptionState;

typedef ArrayBufferOrArrayBufferView BufferSource;

class TextDecoder final : public GarbageCollectedFinalized<TextDecoder>,
                          public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static TextDecoder* Create(const String& label,
                             const TextDecoderOptions&,
                             ExceptionState&);
  ~TextDecoder();

  // Implement the IDL
  String encoding() const;
  bool fatal() const { return fatal_; }
  bool ignoreBOM() const { return ignore_bom_; }
  String decode(const BufferSource&, const TextDecodeOptions&, ExceptionState&);
  String decode(ExceptionState&);

  DEFINE_INLINE_TRACE() {}

 private:
  TextDecoder(const WTF::TextEncoding&, bool fatal, bool ignore_bom);

  String decode(const char* start,
                size_t length,
                const TextDecodeOptions&,
                ExceptionState&);

  WTF::TextEncoding encoding_;
  std::unique_ptr<WTF::TextCodec> codec_;
  bool fatal_;
  bool ignore_bom_;
  bool bom_seen_;
};

}  // namespace blink

#endif  // TextDecoder_h
