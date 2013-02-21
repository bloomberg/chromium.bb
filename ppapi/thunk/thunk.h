// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_THUNK_H_
#define PPAPI_THUNK_THUNK_H_

#include "ppapi/c/private/ppb_instance_private.h"
#include "ppapi/c/trusted/ppb_audio_trusted.h"
#include "ppapi/c/trusted/ppb_buffer_trusted.h"
#include "ppapi/c/trusted/ppb_graphics_3d_trusted.h"
#include "ppapi/c/trusted/ppb_image_data_trusted.h"
#include "ppapi/c/trusted/ppb_url_loader_trusted.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

// Declares a getter for the interface thunk of the form:
//
//   const PPB_Foo* ppapi::thunk::GetPPB_Foo_Thunk();
//
#define IFACE(api_name, interface_name, InterfaceType) \
  struct InterfaceType; \
  namespace ppapi { namespace thunk { \
  PPAPI_THUNK_EXPORT const InterfaceType* Get##InterfaceType##_Thunk(); \
  } }
#define PROXIED_IFACE IFACE
#define UNPROXIED_IFACE IFACE

#include "ppapi/thunk/interfaces_ppb_private.h"
#include "ppapi/thunk/interfaces_ppb_private_no_permissions.h"
#include "ppapi/thunk/interfaces_ppb_private_flash.h"
#include "ppapi/thunk/interfaces_ppb_public_stable.h"
#include "ppapi/thunk/interfaces_ppb_public_dev.h"

#undef UNPROXIED_IFACE
#undef PROXIED_IFACE
#undef IFACE

namespace ppapi {
namespace thunk {

// Old-style thunk getters. Only put trusted/private stuff here (it hasn't
// yet been converted to the new system). Otherwise, add the declaration to
// the appropriate interfaces_*.h file.
PPAPI_THUNK_EXPORT const PPB_AudioTrusted_0_6* GetPPB_AudioTrusted_0_6_Thunk();
PPAPI_THUNK_EXPORT const PPB_BufferTrusted_0_1*
    GetPPB_BufferTrusted_0_1_Thunk();
PPAPI_THUNK_EXPORT const PPB_Graphics3DTrusted_1_0*
    GetPPB_Graphics3DTrusted_1_0_Thunk();
PPAPI_THUNK_EXPORT const PPB_ImageDataTrusted_0_4*
    GetPPB_ImageDataTrusted_0_4_Thunk();
PPAPI_THUNK_EXPORT const PPB_Instance_Private_0_1*
    GetPPB_Instance_Private_0_1_Thunk();
PPAPI_THUNK_EXPORT const PPB_URLLoaderTrusted_0_3*
    GetPPB_URLLoaderTrusted_0_3_Thunk();

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_THUNK_H_
