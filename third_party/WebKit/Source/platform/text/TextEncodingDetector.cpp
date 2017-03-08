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

#include "platform/text/TextEncodingDetector.h"

#include "third_party/ced/src/compact_enc_det/compact_enc_det.h"
#include "wtf/text/TextEncoding.h"

namespace blink {

bool detectTextEncoding(const char* data,
                        size_t length,
                        const char* hintEncodingName,
                        const char* hintUrl,
                        const char* hintUserLanguage,
                        WTF::TextEncoding* detectedEncoding) {
  *detectedEncoding = WTF::TextEncoding();
  Language language;
  LanguageFromCode(hintUserLanguage, &language);
  int consumedBytes;
  bool isReliable;
  Encoding encoding = CompactEncDet::DetectEncoding(
      data, length, hintUrl, nullptr, nullptr,
      EncodingNameAliasToEncoding(hintEncodingName), language,
      CompactEncDet::WEB_CORPUS,
      false,  // Include 7-bit encodings to detect ISO-2022-JP
      &consumedBytes, &isReliable);

  // Should return false if the detected encoding is UTF8. This helps prevent
  // modern web sites from neglecting proper encoding labelling and simply
  // relying on browser-side encoding detection. Encoding detection is supposed
  // to work for web sites with legacy encoding only. Detection failure leads
  // |TextResourceDecoder| to use its default encoding determined from system
  // locale or TLD.
  if (encoding == UNKNOWN_ENCODING || encoding == UTF8)
    return false;

  // Map all the Shift-JIS variants to Shift-JIS.
  if (hintUserLanguage && !strncmp(hintUserLanguage, "ja", 2) &&
      IsShiftJisOrVariant(encoding)) {
    encoding = JAPANESE_SHIFT_JIS;
  }

  // 7-bit encodings (except ISO-2022-JP), and some obscure encodings not
  // supported in WHATWG encoding standard are marked as ASCII to keep the raw
  // bytes intact.
  // TODO(jinsukkim): Put this conversion into CED library, and enable "WHATWG"
  // mode.
  switch (encoding) {
    case HZ_GB_2312:
    case ISO_2022_KR:
    case ISO_2022_CN:
    case UTF7:

    case CHINESE_EUC_DEC:
    case CHINESE_CNS:
    case CHINESE_BIG5_CP950:
    case JAPANESE_CP932:
    case MSFT_CP874:
    case TSCII:
    case TAMIL_MONO:
    case TAMIL_BI:
    case JAGRAN:
    case BHASKAR:
    case HTCHANAKYA:
    case BINARYENC:
    case UTF8UTF8:
    case TAM_ELANGO:
    case TAM_LTTMBARANI:
    case TAM_SHREE:
    case TAM_TBOOMIS:
    case TAM_TMNEWS:
    case TAM_WEBTAMIL:
    case KDDI_SHIFT_JIS:
    case DOCOMO_SHIFT_JIS:
    case SOFTBANK_SHIFT_JIS:
    case KDDI_ISO_2022_JP:
    case SOFTBANK_ISO_2022_JP:
      encoding = ASCII_7BIT;
      break;
    default:
      break;
  }
  *detectedEncoding = WTF::TextEncoding(MimeEncodingName(encoding));
  return true;
}

}  // namespace blink
