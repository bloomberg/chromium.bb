// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_CHAR_SET_API_H_
#define PPAPI_THUNK_PPB_CHAR_SET_API_H_

#include "ppapi/c/dev/ppb_char_set_dev.h"
#include "ppapi/proxy/interface_id.h"

namespace ppapi {
namespace thunk {

class PPB_CharSet_FunctionAPI {
 public:
  static const ::pp::proxy::InterfaceID interface_id =
      ::pp::proxy::INTERFACE_ID_PPB_CHAR_SET;

  virtual char* UTF16ToCharSet(PP_Instance instance,
                               const uint16_t* utf16, uint32_t utf16_len,
                               const char* output_char_set,
                               PP_CharSet_ConversionError on_error,
                               uint32_t* output_length) = 0;
  virtual uint16_t* CharSetToUTF16(PP_Instance instance,
                                   const char* input, uint32_t input_len,
                                   const char* input_char_set,
                                   PP_CharSet_ConversionError on_error,
                                   uint32_t* output_length) = 0;
  virtual PP_Var GetDefaultCharSet(PP_Instance instance) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_CHAR_SET_API_H_
