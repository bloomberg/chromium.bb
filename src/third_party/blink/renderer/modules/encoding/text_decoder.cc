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

#include "third_party/blink/renderer/modules/encoding/text_decoder.h"

#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/modules/encoding/encoding.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding_registry.h"

namespace blink {

TextDecoder* TextDecoder::Create(const String& label,
                                 const TextDecoderOptions* options,
                                 ExceptionState& exception_state) {
  WTF::TextEncoding encoding(
      label.StripWhiteSpace(&encoding::IsASCIIWhiteSpace));
  // The replacement encoding is not valid, but the Encoding API also
  // rejects aliases of the replacement encoding.
  if (!encoding.IsValid() ||
      WTF::EqualIgnoringASCIICase(encoding.GetName(), "replacement")) {
    exception_state.ThrowRangeError("The encoding label provided ('" + label +
                                    "') is invalid.");
    return nullptr;
  }

  return MakeGarbageCollected<TextDecoder>(encoding, options->fatal(),
                                           options->ignoreBOM());
}

TextDecoder::TextDecoder(const WTF::TextEncoding& encoding,
                         bool fatal,
                         bool ignore_bom)
    : encoding_(encoding),
      fatal_(fatal),
      ignore_bom_(ignore_bom),
      bom_seen_(false) {}

TextDecoder::~TextDecoder() = default;

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
                           const TextDecodeOptions* options,
                           ExceptionState& exception_state) {
  DCHECK(options);
  DCHECK(!input.IsNull());
  if (input.IsArrayBufferView()) {
    const char* start = static_cast<const char*>(
        input.GetAsArrayBufferView().View()->BaseAddress());
    size_t length = input.GetAsArrayBufferView().View()->byteLengthAsSizeT();
    if (length > std::numeric_limits<uint32_t>::max()) {
      exception_state.ThrowRangeError(
          "Buffer size exceeds maximum heap object size.");
      return String();
    }
    return decode(start, static_cast<uint32_t>(length), options,
                  exception_state);
  }
  DCHECK(input.IsArrayBuffer());
  const char* start =
      static_cast<const char*>(input.GetAsArrayBuffer()->Data());
  size_t length = input.GetAsArrayBuffer()->ByteLengthAsSizeT();
  if (length > std::numeric_limits<uint32_t>::max()) {
    exception_state.ThrowRangeError(
        "Buffer size exceeds maximum heap object size.");
    return String();
  }
  return decode(start, static_cast<uint32_t>(length), options, exception_state);
}

String TextDecoder::decode(const char* start,
                           uint32_t length,
                           const TextDecodeOptions* options,
                           ExceptionState& exception_state) {
  DCHECK(options);
  if (!do_not_flush_) {
    codec_ = NewTextCodec(encoding_);
    bom_seen_ = false;
  }

  DCHECK(codec_);
  do_not_flush_ = options->stream();
  WTF::FlushBehavior flush = do_not_flush_ ? WTF::FlushBehavior::kDoNotFlush
                                           : WTF::FlushBehavior::kDataEOF;

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

  return s;
}

String TextDecoder::decode(ExceptionState& exception_state) {
  TextDecodeOptions* options = TextDecodeOptions::Create();
  return decode(nullptr, 0, options, exception_state);
}

}  // namespace blink
