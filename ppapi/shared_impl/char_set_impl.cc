// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/char_set_impl.h"

#include "base/i18n/icu_string_conversions.h"
#include "ppapi/c/ppb_core.h"
#include "unicode/ucnv.h"
#include "unicode/ucnv_cb.h"
#include "unicode/ucnv_err.h"
#include "unicode/ustring.h"

namespace pp {
namespace shared_impl {

namespace {

// Converts the given PP error handling behavior to the version in base,
// placing the result in |*result| and returning true on success. Returns false
// if the enum is invalid.
bool PPToBaseConversionError(PP_CharSet_ConversionError on_error,
                             base::OnStringConversionError::Type* result) {
  switch (on_error) {
    case PP_CHARSET_CONVERSIONERROR_FAIL:
      *result = base::OnStringConversionError::FAIL;
      return true;
    case PP_CHARSET_CONVERSIONERROR_SKIP:
      *result = base::OnStringConversionError::SKIP;
      return true;
    case PP_CHARSET_CONVERSIONERROR_SUBSTITUTE:
      *result = base::OnStringConversionError::SUBSTITUTE;
      return true;
    default:
      return false;
  }
}

}  // namespace

// static
// The "substitution" behavior of this function does not match the
// implementation in base, so we partially duplicate the code from
// icu_string_conversions.cc with the correct error handling setup required
// by the PPAPI interface.
char* CharSetImpl::UTF16ToCharSet(const PPB_Core* core,
                                  const uint16_t* utf16,
                                  uint32_t utf16_len,
                                  const char* output_char_set,
                                  PP_CharSet_ConversionError on_error,
                                  uint32_t* output_length) {
  if (!core || !utf16 || !output_char_set || !output_length)
    return NULL;

  *output_length = 0;

  UErrorCode status = U_ZERO_ERROR;
  UConverter* converter = ucnv_open(output_char_set, &status);
  if (!U_SUCCESS(status))
    return NULL;

  int encoded_max_length = UCNV_GET_MAX_BYTES_FOR_STRING(utf16_len,
      ucnv_getMaxCharSize(converter));

  // Setup our error handler.
  switch (on_error) {
    case PP_CHARSET_CONVERSIONERROR_FAIL:
      ucnv_setFromUCallBack(converter, UCNV_FROM_U_CALLBACK_STOP, 0,
                            NULL, NULL, &status);
      break;
    case PP_CHARSET_CONVERSIONERROR_SKIP:
      ucnv_setFromUCallBack(converter, UCNV_FROM_U_CALLBACK_SKIP, 0,
                            NULL, NULL, &status);
      break;
    case PP_CHARSET_CONVERSIONERROR_SUBSTITUTE: {
      // ICU sets the substitution char for some character sets (like latin1)
      // to be the ASCII "substitution character" (26). We want to use '?'
      // instead for backwards-compat with Windows behavior.
      char subst_chars[32];
      int8_t subst_chars_len = 32;
      ucnv_getSubstChars(converter, subst_chars, &subst_chars_len, &status);
      if (subst_chars_len == 1 && subst_chars[0] == 26) {
        // Override to the question mark character if possible. When using
        // setSubstString, the input is a Unicode character. The function will
        // try to convert it to the destination character set and fail if that
        // can not be converted to the destination character set.
        //
        // We just ignore any failure. If the dest char set has no
        // representation for '?', then we'll just stick to the ICU default
        // substitution character.
        UErrorCode subst_status = U_ZERO_ERROR;
        UChar question_mark = '?';
        ucnv_setSubstString(converter, &question_mark, 1, &subst_status);
      }

      ucnv_setFromUCallBack(converter, UCNV_FROM_U_CALLBACK_SUBSTITUTE, 0,
                            NULL, NULL, &status);
      break;
    }
    default:
      return NULL;
  }

  // ucnv_fromUChars returns size not including terminating null.
  char* encoded = static_cast<char*>(core->MemAlloc(encoded_max_length + 1));
  int actual_size = ucnv_fromUChars(converter, encoded,
      encoded_max_length, reinterpret_cast<const UChar*>(utf16), utf16_len,
      &status);
  ucnv_close(converter);
  if (!U_SUCCESS(status)) {
    core->MemFree(encoded);
    return NULL;
  }
  encoded[actual_size] = 0;
  *output_length = actual_size;
  return encoded;
}

// static
uint16_t* CharSetImpl::CharSetToUTF16(const PPB_Core* core,
                                      const char* input,
                                      uint32_t input_len,
                                      const char* input_char_set,
                                      PP_CharSet_ConversionError on_error,
                                      uint32_t* output_length) {
  if (!core || !input || !input_char_set || !output_length)
    return NULL;

  *output_length = 0;

  base::OnStringConversionError::Type base_on_error;
  if (!PPToBaseConversionError(on_error, &base_on_error))
    return NULL;  // Invalid enum value.

  // We can convert this call to the implementation in base to avoid code
  // duplication, although this does introduce an extra copy of the data.
  string16 output;
  if (!base::CodepageToUTF16(std::string(input, input_len), input_char_set,
                             base_on_error, &output))
    return NULL;

  uint16_t* ret_buf = static_cast<uint16_t*>(
      core->MemAlloc((output.size() + 1) * sizeof(uint16_t)));
  if (!ret_buf)
    return NULL;

  *output_length = static_cast<uint32_t>(output.size());
  memcpy(ret_buf, output.c_str(), (output.size() + 1) * sizeof(uint16_t));
  return ret_buf;
}

}  // namespace shared_impl
}  // namespace pp
