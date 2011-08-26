// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_CHAR_SET_IMPL_H_
#define PPAPI_SHARED_IMPL_CHAR_SET_IMPL_H_

#include "base/basictypes.h"
#include "ppapi/c/dev/ppb_char_set_dev.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

struct PPB_Memory_Dev;

namespace ppapi {

// Contains the implementation of character set conversion that is shared
// between the proxy and the renderer.
class PPAPI_SHARED_EXPORT CharSetImpl {
 public:
  static char* UTF16ToCharSet(const PPB_Memory_Dev* memory,
                              const uint16_t* utf16,
                              uint32_t utf16_len,
                              const char* output_char_set,
                              PP_CharSet_ConversionError on_error,
                              uint32_t* output_length);

  static uint16_t* CharSetToUTF16(const PPB_Memory_Dev* memory,
                                  const char* input,
                                  uint32_t input_len,
                                  const char* input_char_set,
                                  PP_CharSet_ConversionError on_error,
                                  uint32_t* output_length);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_CHAR_SET_IMPL_H_
