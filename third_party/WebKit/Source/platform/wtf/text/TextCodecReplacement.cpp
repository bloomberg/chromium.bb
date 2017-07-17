// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/wtf/text/TextCodecReplacement.h"

#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/text/CharacterNames.h"
#include "platform/wtf/text/WTFString.h"
#include <memory>

namespace WTF {

TextCodecReplacement::TextCodecReplacement()
    : replacement_error_returned_(false) {}

void TextCodecReplacement::RegisterEncodingNames(
    EncodingNameRegistrar registrar) {
  // Taken from the alias table at·https://encoding.spec.whatwg.org/
  registrar("replacement", "replacement");
  registrar("csiso2022kr", "replacement");
  registrar("hz-gb-2312", "replacement");
  registrar("iso-2022-cn", "replacement");
  registrar("iso-2022-cn-ext", "replacement");
  registrar("iso-2022-kr", "replacement");
}

static std::unique_ptr<TextCodec> NewStreamingTextDecoderReplacement(
    const TextEncoding&,
    const void*) {
  return WTF::WrapUnique(new TextCodecReplacement);
}

void TextCodecReplacement::RegisterCodecs(TextCodecRegistrar registrar) {
  registrar("replacement", NewStreamingTextDecoderReplacement, 0);
}

String TextCodecReplacement::Decode(const char*,
                                    size_t length,
                                    FlushBehavior,
                                    bool,
                                    bool& saw_error) {
  // https://encoding.spec.whatwg.org/#replacement-decoder

  // 1. If byte is end-of-stream, return finished.
  if (!length)
    return String();

  // 2. If replacement error returned flag is unset, set the replacement
  // error returned flag and return error.
  if (!replacement_error_returned_) {
    replacement_error_returned_ = true;
    saw_error = true;
    return String(&kReplacementCharacter, 1);
  }

  // 3. Return finished.
  return String();
}

}  // namespace WTF
