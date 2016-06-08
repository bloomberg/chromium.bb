// Copyright 2016 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////////

#include "util/encodings/encodings.h"

#include <string.h>                     // for strcasecmp
#include <utility>                      // for pair

#include "util/basictypes.h"
#include "util/string_util.h"

struct EncodingInfo {
  // The standard name for this encoding.
  //
  const char* encoding_name_;

  // The "preferred MIME name" of an encoding as specified by the IANA at:
  //     http://www.iana.org/assignments/character-sets
  //
  //   Note that the preferred MIME name may differ slightly from the
  //   official IANA name: i.e. ISO-8859-1 vs. ISO_8859-1:1987
  //
  const char* mime_encoding_name_;

  // It is an internal policy that if an encoding has an IANA name,
  // then encoding_name_ and mime_encoding_name_ must be the same string.
  //
  // However, there can be exceptions if there are compelling reasons.
  // For example, Japanese mobile handsets require the name
  // "Shift_JIS" in charset=... parameter in Content-Type headers to
  // process emoji (emoticons) in their private encodings.  In that
  // case, mime_encoding_name_ should be "Shift_JIS", despite
  // encoding_name_ actually is "X-KDDI-Shift_JIS".

  // Some multi-byte encodings use byte values that coincide with the
  // ASCII codes for HTML syntax characters <>"&' and browsers like MSIE
  // can misinterpret these, as indicated in an external XSS report from
  // 2007-02-15. Here, we map these dangerous encodings to safer ones. We
  // also use UTF8 instead of encodings that we don't support in our
  // output, and we generally try to be conservative in what we send out.
  // Where the client asks for single- or double-byte encodings that are
  // not as common, we substitute a more common single- or double-byte
  // encoding, if there is one, thereby preserving the client's intent
  // to use less space than UTF-8. This also means that characters
  // outside the destination set will be converted to HTML NCRs (&#NNN;)
  // if requested.

  Encoding preferred_web_output_encoding_;
};

static const EncodingInfo kEncodingInfoTable[] = {
  { "ASCII", "ISO-8859-1", ISO_8859_1},
  { "Latin2", "ISO-8859-2", ISO_8859_2},
  { "Latin3", "ISO-8859-3", UTF8},
      // MSIE 6 does not support ISO-8859-3 (XSS issue)
  { "Latin4", "ISO-8859-4", ISO_8859_4},
  { "ISO-8859-5", "ISO-8859-5", ISO_8859_5},
  { "Arabic", "ISO-8859-6", ISO_8859_6},
  { "Greek", "ISO-8859-7", ISO_8859_7},
  { "Hebrew", "ISO-8859-8", MSFT_CP1255},
      // we do not endorse the visual order
  { "Latin5", "ISO-8859-9", ISO_8859_9},
  { "Latin6", "ISO-8859-10", UTF8},
      // MSIE does not support ISO-8859-10 (XSS issue)
  { "EUC-JP",  "EUC-JP", JAPANESE_EUC_JP},
  { "SJS", "Shift_JIS", JAPANESE_SHIFT_JIS},
  { "JIS", "ISO-2022-JP", JAPANESE_SHIFT_JIS},
      // due to potential confusion with HTML syntax chars
  { "BIG5", "Big5", CHINESE_BIG5},
  { "GB",  "GB2312", CHINESE_GB},
  { "EUC-CN",
        "EUC-CN",
        // Misnamed. Should be EUC-TW.
        CHINESE_BIG5},
      // MSIE treats "EUC-CN" like GB2312, which is not EUC-TW,
      // and EUC-TW is rare, so we prefer Big5 for output.
  { "KSC", "EUC-KR", KOREAN_EUC_KR},
  { "Unicode",
    "UTF-16LE",
        // Internet Explorer doesn't recognize "ISO-10646-UCS-2"
        UTF8
        // due to potential confusion with HTML syntax chars
        },
  { "EUC",
        "EUC",  // Misnamed. Should be EUC-TW.
        CHINESE_BIG5
        // MSIE does not recognize "EUC" (XSS issue),
        // and EUC-TW is rare, so we prefer Big5 for output.
        },
  { "CNS",
        "CNS",  // Misnamed. Should be EUC-TW.
        CHINESE_BIG5},
      // MSIE does not recognize "CNS" (XSS issue),
      // and EUC-TW is rare, so we prefer Big5 for output.
  { "BIG5-CP950",
        "BIG5-CP950",  // Not an IANA name
        CHINESE_BIG5
        // MSIE does not recognize "BIG5-CP950" (XSS issue)
        },
  { "CP932", "CP932",  // Not an IANA name
        JAPANESE_SHIFT_JIS},  // MSIE does not recognize "CP932" (XSS issue)
  { "UTF8", "UTF-8", UTF8},
  { "Unknown",
        "x-unknown",  // Not an IANA name
        UTF8},  // UTF-8 is our default output encoding
  { "ASCII-7-bit", "US-ASCII", ASCII_7BIT},
  { "KOI8R", "KOI8-R", RUSSIAN_KOI8_R},
  { "CP1251", "windows-1251", RUSSIAN_CP1251},
  { "CP1252", "windows-1252", MSFT_CP1252},
  { "KOI8U",
        "KOI8-U",
        ISO_8859_5},  // because koi8-u is not as common
  { "CP1250", "windows-1250", MSFT_CP1250},
  { "ISO-8859-15", "ISO-8859-15", ISO_8859_15},
  { "CP1254", "windows-1254", MSFT_CP1254},
  { "CP1257", "windows-1257", MSFT_CP1257},
  { "ISO-8859-11", "ISO-8859-11", ISO_8859_11},
  { "CP874", "windows-874", MSFT_CP874},
  { "CP1256", "windows-1256", MSFT_CP1256},
  { "CP1255", "windows-1255", MSFT_CP1255},
  { "ISO-8859-8-I", "ISO-8859-8-I", MSFT_CP1255},
      // Java does not support iso-8859-8-i
  { "VISUAL", "ISO-8859-8", MSFT_CP1255},
      // we do not endorse the visual order
  { "CP852", "cp852", MSFT_CP1250},
      // because cp852 is not as common
  { "CSN_369103", "csn_369103", MSFT_CP1250},
      // MSIE does not recognize "csn_369103" (XSS issue)
  { "CP1253", "windows-1253", MSFT_CP1253},
  { "CP866", "IBM866", RUSSIAN_CP1251},
      // because cp866 is not as common
  { "ISO-8859-13", "ISO-8859-13", UTF8},
      // because iso-8859-13 is not widely supported
  { "ISO-2022-KR", "ISO-2022-KR", KOREAN_EUC_KR},
      // due to potential confusion with HTML syntax chars
  { "GBK", "GBK", GBK},
  { "GB18030", "GB18030", GBK},
      // because gb18030 is not widely supported
  { "BIG5_HKSCS", "BIG5-HKSCS", CHINESE_BIG5},
      // because Big5-HKSCS is not widely supported
  { "ISO_2022_CN", "ISO-2022-CN", CHINESE_GB},
      // due to potential confusion with HTML syntax chars
  { "TSCII", "tscii", UTF8},
      // we do not have an output converter for this font encoding
  { "TAM", "tam", UTF8},
      // we do not have an output converter for this font encoding
  { "TAB", "tab", UTF8},
      // we do not have an output converter for this font encoding
  { "JAGRAN", "jagran", UTF8},
      // we do not have an output converter for this font encoding
  { "MACINTOSH", "MACINTOSH", ISO_8859_1},
      // because macintosh is relatively uncommon
  { "UTF7", "UTF-7",
        UTF8},  // UTF-7 has been the subject of XSS attacks and is deprecated
  { "BHASKAR", "bhaskar",
        UTF8},  // we do not have an output converter for this font encoding
  { "HTCHANAKYA", "htchanakya",  // not an IANA charset name.
        UTF8},  // we do not have an output converter for this font encoding
  { "UTF-16BE", "UTF-16BE",
        UTF8},  // due to potential confusion with HTML syntax chars
  { "UTF-16LE", "UTF-16LE",
        UTF8},  // due to potential confusion with HTML syntax chars
  { "UTF-32BE", "UTF-32BE",
        UTF8},  // unlikely to cause XSS bugs, but very uncommon on Web
  { "UTF-32LE", "UTF-32LE",
        UTF8},  // unlikely to cause XSS bugs, but very uncommon on Web
  { "X-BINARYENC", "x-binaryenc",  // Not an IANA name
        UTF8},  // because this one is not intended for output (just input)
  { "HZ-GB-2312", "HZ-GB-2312",
        CHINESE_GB},  // due to potential confusion with HTML syntax chars
  { "X-UTF8UTF8", "x-utf8utf8",  // Not an IANA name
        UTF8},  // because this one is not intended for output (just input)
  { "X-TAM-ELANGO", "x-tam-elango",
        UTF8},  // we do not have an output converter for this font encoding
  { "X-TAM-LTTMBARANI", "x-tam-lttmbarani",
        UTF8},  // we do not have an output converter for this font encoding
  { "X-TAM-SHREE", "x-tam-shree",
        UTF8},  // we do not have an output converter for this font encoding
  { "X-TAM-TBOOMIS", "x-tam-tboomis",
        UTF8},  // we do not have an output converter for this font encoding
  { "X-TAM-TMNEWS", "x-tam-tmnews",
        UTF8},  // we do not have an output converter for this font encoding
  { "X-TAM-WEBTAMIL", "x-tam-webtamil",
        UTF8},  // we do not have an output converter for this font encoding

  { "X-KDDI-Shift_JIS", "Shift_JIS", JAPANESE_SHIFT_JIS},
      // KDDI version of Shift_JIS with Google Emoji PUA mappings.
      // Note that MimeEncodingName() returns "Shift_JIS", since KDDI uses
      // "Shift_JIS" in HTTP headers and email messages.

  { "X-DoCoMo-Shift_JIS", "Shift_JIS", JAPANESE_SHIFT_JIS},
      // DoCoMo version of Shift_JIS with Google Emoji PUA mappings.
      // See the comment at KDDI_SHIFT_JIS for other issues.

  { "X-SoftBank-Shift_JIS", "Shift_JIS", JAPANESE_SHIFT_JIS},
      // SoftBank version of Shift_JIS with Google Emoji PUA mappings.
      // See the comment at KDDI_SHIFT_JIS for other issues.

  { "X-KDDI-ISO-2022-JP", "ISO-2022-JP", JAPANESE_SHIFT_JIS},
      // KDDI version of ISO-2022-JP with Google Emoji PUA mappings.
      // See the comment at KDDI_SHIFT_JIS for other issues.
      // The preferred Web encoding is due to potential confusion with
      // HTML syntax chars.

  { "X-SoftBank-ISO-2022-JP", "ISO-2022-JP", JAPANESE_SHIFT_JIS},
      // SoftBank version of ISO-2022-JP with Google Emoji PUA mappings.
      // See the comment at KDDI_SHIFT_JIS for other issues.
      // The preferred Web encoding is due to potential confusion with
      // HTML syntax chars.

      // Please refer to NOTE: section in the comments in the definition
      // of "struct I18NInfoByEncoding", before adding new encodings.

};



COMPILE_ASSERT(arraysize(kEncodingInfoTable) == NUM_ENCODINGS,
               kEncodingInfoTable_has_incorrect_size);

Encoding default_encoding() {return LATIN1;}

// *************************************************************
// Encoding predicates
//   IsValidEncoding()
//   IsEncEncCompatible
//   IsEncodingWithSupportedLanguage
//   IsSupersetOfAscii7Bit
//   Is8BitEncoding
//   IsCJKEncoding
//   IsHebrewEncoding
//   IsRightToLeftEncoding
//   IsLogicalRightToLeftEncoding
//   IsVisualRightToLeftEncoding
//   IsIso2022Encoding
//   IsIso2022JpOrVariant
//   IsShiftJisOrVariant
//   IsJapaneseCellPhoneCarrierSpecificEncoding
// *************************************************************

bool IsValidEncoding(Encoding enc) {
  return ((enc >= 0) && (enc < kNumEncodings));
}

bool IsEncEncCompatible(const Encoding from, const Encoding to) {
  // Tests compatibility between the "from" and "to" encodings; in
  // the typical case -- when both are valid known encodings -- this
  // returns true iff converting from first to second is a no-op.
  if (!IsValidEncoding(from) || !IsValidEncoding(to)) {
    return false;  // we only work with valid encodings...
  } else if (to == from) {
    return true;   // the trivial common case
  }

  if (to == UNKNOWN_ENCODING) {
    return true;   // all valid encodings are compatible with the unknown
  }

  if (from == UNKNOWN_ENCODING) {
    return false;  // no unknown encoding is compatible with one that is
  }

  if (from == ASCII_7BIT) {
    return IsSupersetOfAscii7Bit(to);
  }

  return (from == ISO_8859_1 && to == MSFT_CP1252) ||
         (from == ISO_8859_8 && to == HEBREW_VISUAL) ||
         (from == HEBREW_VISUAL && to == ISO_8859_8) ||
         (from == ISO_8859_9 && to == MSFT_CP1254) ||
         (from == ISO_8859_11 && to == MSFT_CP874) ||
         (from == JAPANESE_SHIFT_JIS && to == JAPANESE_CP932) ||
         (from == CHINESE_BIG5 && to == CHINESE_BIG5_CP950) ||
         (from == CHINESE_GB && to == GBK) ||
         (from == CHINESE_GB && to == GB18030) ||
         (from == CHINESE_EUC_CN && to == CHINESE_EUC_DEC) ||
         (from == CHINESE_EUC_CN && to == CHINESE_CNS) ||
         (from == CHINESE_EUC_DEC && to == CHINESE_EUC_CN) ||
         (from == CHINESE_EUC_DEC && to == CHINESE_CNS) ||
         (from == CHINESE_CNS && to == CHINESE_EUC_CN) ||
         (from == CHINESE_CNS && to == CHINESE_EUC_DEC);
}

// To be a superset of 7-bit Ascii means that bytes 0...127 in the given
// encoding represent the same characters as they do in ISO_8859_1.

// TODO: This list could be expanded.  Many other encodings are supersets
// of 7-bit Ascii.  In fact, Japanese JIS and Unicode are the only two
// encodings that I know for a fact should *not* be in this list.
bool IsSupersetOfAscii7Bit(Encoding e) {
  switch (e) {
    case ISO_8859_1:
    case ISO_8859_2:
    case ISO_8859_3:
    case ISO_8859_4:
    case ISO_8859_5:
    case ISO_8859_6:
    case ISO_8859_7:
    case ISO_8859_8:
    case ISO_8859_9:
    case ISO_8859_10:
    case JAPANESE_EUC_JP:
    case JAPANESE_SHIFT_JIS:
    case CHINESE_BIG5:
    case CHINESE_GB:
    case CHINESE_EUC_CN:
    case KOREAN_EUC_KR:
    case CHINESE_EUC_DEC:
    case CHINESE_CNS:
    case CHINESE_BIG5_CP950:
    case JAPANESE_CP932:
    case UTF8:
    case UNKNOWN_ENCODING:
    case ASCII_7BIT:
    case RUSSIAN_KOI8_R:
    case RUSSIAN_CP1251:
    case MSFT_CP1252:
    case RUSSIAN_KOI8_RU:
    case MSFT_CP1250:
    case ISO_8859_15:
    case MSFT_CP1254:
    case MSFT_CP1257:
    case ISO_8859_11:
    case MSFT_CP874:
    case MSFT_CP1256:
    case MSFT_CP1255:
    case ISO_8859_8_I:
    case HEBREW_VISUAL:
    case CZECH_CP852:
    case MSFT_CP1253:
    case RUSSIAN_CP866:
    case ISO_8859_13:
    case GBK:
    case GB18030:
    case BIG5_HKSCS:
    case MACINTOSH_ROMAN:
      return true;
    default:
      return false;
  }
}

// To be an 8-bit encoding means that there are fewer than 256 symbols.
// Each byte determines a new character; there are no multi-byte sequences.

// TODO: This list could maybe be expanded.  Other encodings may be 8-bit.
bool Is8BitEncoding(Encoding e) {
  switch (e) {
    case ASCII_7BIT:
    case ISO_8859_1:
    case ISO_8859_2:
    case ISO_8859_3:
    case ISO_8859_4:
    case ISO_8859_5:
    case ISO_8859_6:
    case ISO_8859_7:
    case ISO_8859_8:
    case ISO_8859_8_I:
    case ISO_8859_9:
    case ISO_8859_10:
    case ISO_8859_11:
    case ISO_8859_13:
    case ISO_8859_15:
    case MSFT_CP1252:
    case MSFT_CP1253:
    case MSFT_CP1254:
    case MSFT_CP1255:
    case MSFT_CP1256:
    case MSFT_CP1257:
    case RUSSIAN_KOI8_R:
    case RUSSIAN_KOI8_RU:
    case RUSSIAN_CP866:
      return true;
    default:
      return false;
  }
}

bool IsCJKEncoding(Encoding e) {
  switch (e) {
    case JAPANESE_EUC_JP:
    case JAPANESE_SHIFT_JIS:
    case JAPANESE_JIS:
    case CHINESE_BIG5:
    case CHINESE_GB:
    case CHINESE_EUC_CN:
    case KOREAN_EUC_KR:
    case CHINESE_EUC_DEC:
    case CHINESE_CNS:
    case CHINESE_BIG5_CP950:
    case JAPANESE_CP932:
    case ISO_2022_KR:
    case GBK:
    case GB18030:
    case BIG5_HKSCS:
    case ISO_2022_CN:
    case HZ_GB_2312:
      return true;
    default:
      return false;
  }
}

bool IsHebrewEncoding(Encoding e) {
  return (e == ISO_8859_8 ||
          e == ISO_8859_8_I ||
          e == MSFT_CP1255 ||
          e == HEBREW_VISUAL);
}



bool IsRightToLeftEncoding(Encoding enc) {
  switch (enc) {
    case MSFT_CP1255:
    case MSFT_CP1256:
    case ARABIC_ENCODING:
    case HEBREW_ENCODING:
    case ISO_8859_8_I:
    case HEBREW_VISUAL:
      return true;
    default:
      return false;
  }
}

bool IsLogicalRightToLeftEncoding(Encoding enc) {
  return IsRightToLeftEncoding(enc) && !IsVisualRightToLeftEncoding(enc);
}

// Note that despite an RFC to the contrary, ARABIC_ENCODING (ISO-8859-6)
// is NOT visual.
bool IsVisualRightToLeftEncoding(Encoding enc) {
  switch (enc) {
    case HEBREW_ENCODING:
    case HEBREW_VISUAL:
      return true;
    default:
      return false;
  }
}





bool IsIso2022Encoding(Encoding enc) {
  return (IsIso2022JpOrVariant(enc) ||
          enc == ISO_2022_KR ||
          enc == ISO_2022_CN);
}

bool IsIso2022JpOrVariant(Encoding enc) {
  return (enc == JAPANESE_JIS ||
          enc == KDDI_ISO_2022_JP ||
          enc == SOFTBANK_ISO_2022_JP);
}

bool IsShiftJisOrVariant(Encoding enc) {
  return (enc == JAPANESE_SHIFT_JIS ||
          enc == JAPANESE_CP932 ||
          enc == KDDI_SHIFT_JIS ||
          enc == DOCOMO_SHIFT_JIS ||
          enc == SOFTBANK_SHIFT_JIS);
}

bool IsJapaneseCellPhoneCarrierSpecificEncoding(Encoding enc) {
  return (enc == KDDI_ISO_2022_JP ||
          enc == KDDI_SHIFT_JIS ||
          enc == DOCOMO_SHIFT_JIS ||
          enc == SOFTBANK_SHIFT_JIS ||
          enc == SOFTBANK_ISO_2022_JP);
}


// *************************************************************
// ENCODING NAMES
//   EncodingName() [Encoding to name]
//   MimeEncodingName() [Encoding to name]
//   EncodingFromName() [name to Encoding]
//   EncodingNameAliasToEncoding() [name to Encoding]
//   default_encoding_name()
//   invalid_encoding_name()
// *************************************************************

const char * EncodingName(const Encoding enc) {
  if ( (enc < 0) || (enc >= kNumEncodings) )
    return invalid_encoding_name();
  return kEncodingInfoTable[enc].encoding_name_;
}

// TODO: Unify MimeEncodingName and EncodingName, or determine why
// such a unification is not possible.

const char * MimeEncodingName(Encoding enc) {
  if ( (enc < 0) || (enc >= kNumEncodings) )
    return "";  // TODO: Should this be invalid_encoding_name()?
  return kEncodingInfoTable[enc].mime_encoding_name_;
}

bool EncodingFromName(const char* enc_name, Encoding *encoding) {
  *encoding = UNKNOWN_ENCODING;
  if ( enc_name == NULL ) return false;

  for ( int i = 0; i < kNumEncodings; i++ ) {
    if (!base::strcasecmp(enc_name, kEncodingInfoTable[i].encoding_name_) ) {
      *encoding = static_cast<Encoding>(i);
      return true;
    }
  }
  return false;
}

const char* default_encoding_name() {
  return kEncodingInfoTable[LATIN1].encoding_name_;
}

static const char* const kInvalidEncodingName = "invalid_encoding";

const char *invalid_encoding_name() {
  return kInvalidEncodingName;
}



// *************************************************************
// Miscellany
// *************************************************************


Encoding PreferredWebOutputEncoding(Encoding enc) {
  return IsValidEncoding(enc)
      ? kEncodingInfoTable[enc].preferred_web_output_encoding_
      : UTF8;
}
