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

#include "modules/encoding/TextDecoder.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMArrayBufferView.h"
#include "modules/encoding/Encoding.h"
#include "platform/wtf/StringExtras.h"
#include "platform/wtf/text/TextEncodingRegistry.h"

namespace blink {

TextDecoder* TextDecoder::Create(const String& label,
                                 const TextDecoderOptions& options,
                                 ExceptionState& exception_state) {
  WTF::TextEncoding encoding(
      label.StripWhiteSpace(&Encoding::IsASCIIWhiteSpace));
  // The replacement encoding is not valid, but the Encoding API also
  // rejects aliases of the replacement encoding.
  if (!encoding.IsValid() || !strcasecmp(encoding.GetName(), "replacement")) {
    exception_state.ThrowRangeError("The encoding label provided ('" + label +
                                    "') is invalid.");
    return 0;
  }

  return new TextDecoder(encoding, options.fatal(), options.ignoreBOM());
}

TextDecoder::TextDecoder(const WTF::TextEncoding& encoding,
                         bool fatal,
                         bool ignore_bom)
    : encoding_(encoding),
      codec_(NewTextCodec(encoding)),
      fatal_(fatal),
      ignore_bom_(ignore_bom),
      bom_seen_(false) {}

TextDecoder::~TextDecoder() {}

String TextDecoder::encoding() const {
  String name = String(encoding_.GetName()).DeprecatedLower();
  // Where possible, encoding aliases should be handled by changes to Chromium's
  // ICU or Blink's WTF.  The same codec is used, but WTF maintains a different
  // name/identity for these.
  if (name == "iso-8859-1" || name == "us-ascii")
    return "windows-1252";
  return name;
}

String TextDecoder::decode(const BufferSource& input,
                           const TextDecodeOptions& options,
                           ExceptionState& exception_state) {
  DCHECK(!input.isNull());
  if (input.isArrayBufferView()) {
    const char* start = static_cast<const char*>(
        input.getAsArrayBufferView().View()->BaseAddress());
    size_t length = input.getAsArrayBufferView().View()->byteLength();
    return decode(start, length, options, exception_state);
  }
  DCHECK(input.isArrayBuffer());
  const char* start =
      static_cast<const char*>(input.getAsArrayBuffer()->Data());
  size_t length = input.getAsArrayBuffer()->ByteLength();
  return decode(start, length, options, exception_state);
}

String TextDecoder::decode(const char* start,
                           size_t length,
                           const TextDecodeOptions& options,
                           ExceptionState& exception_state) {
  WTF::FlushBehavior flush =
      options.stream() ? WTF::kDoNotFlush : WTF::kDataEOF;

  bool saw_error = false;
  String s = codec_->Decode(start, length, flush, fatal_, saw_error);

  if (fatal_ && saw_error) {
    exception_state.ThrowTypeError("The encoded data was not valid.");
    return String();
  }

  if (!ignore_bom_ && !bom_seen_ && !s.IsEmpty()) {
    bom_seen_ = true;
    String name(encoding_.GetName());
    if ((name == "UTF-8" || name == "UTF-16LE" || name == "UTF-16BE") &&
        s[0] == 0xFEFF)
      s.Remove(0);
  }

  if (flush)
    bom_seen_ = false;

  return s;
}

String TextDecoder::decode(ExceptionState& exception_state) {
  TextDecodeOptions options;
  return decode(nullptr, 0, options, exception_state);
}

}  // namespace blink
