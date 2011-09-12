// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Please see inteface_ppb_public_stable for the documentation on the format of
// this file.

#include "ppapi/thunk/interfaces_preamble.h"

PROXIED_API(PPB_Buffer)
PROXIED_API(PPB_CharSet)
PROXIED_API(PPB_Context3D)
PROXIED_API(PPB_CursorControl)
PROXIED_API(PPB_FileChooser)
PROXIED_API(PPB_Font)
PROXIED_API(PPB_Graphics3D)
PROXIED_API(PPB_Surface3D)
PROXIED_API(PPB_VideoCapture)
PROXIED_API(PPB_VideoDecoder)

PROXIED_IFACE(PPB_Buffer, PPB_BUFFER_DEV_INTERFACE_0_4, PPB_Buffer_Dev)
PROXIED_IFACE(PPB_CharSet, PPB_CHAR_SET_DEV_INTERFACE_0_4, PPB_CharSet_Dev)
PROXIED_IFACE(PPB_Context3D, PPB_CONTEXT_3D_DEV_INTERFACE_0_1,
              PPB_Context3D_Dev)
PROXIED_IFACE(PPB_Context3D, PPB_GLES_CHROMIUM_TEXTURE_MAPPING_DEV_INTERFACE,
              PPB_GLESChromiumTextureMapping_Dev)
PROXIED_IFACE(PPB_CursorControl, PPB_CURSOR_CONTROL_DEV_INTERFACE_0_4,
              PPB_CursorControl_Dev)
PROXIED_IFACE(PPB_FileChooser, PPB_FILECHOOSER_DEV_INTERFACE_0_5,
              PPB_FileChooser_Dev)
PROXIED_IFACE(PPB_Font, PPB_FONT_DEV_INTERFACE_0_6, PPB_Font_Dev)
PROXIED_IFACE(PPB_Instance, PPB_CONSOLE_DEV_INTERFACE, PPB_Console_Dev)
PROXIED_IFACE(PPB_Instance, PPB_MOUSELOCK_DEV_INTERFACE_0_1,
              PPB_MouseLock_Dev)
PROXIED_IFACE(PPB_Surface3D, PPB_SURFACE_3D_DEV_INTERFACE_0_2,
              PPB_Surface3D_Dev)
PROXIED_IFACE(PPB_VideoCapture, PPB_VIDEO_CAPTURE_DEV_INTERFACE_0_1,
              PPB_VideoCapture_Dev)
PROXIED_IFACE(PPB_VideoDecoder, PPB_VIDEODECODER_DEV_INTERFACE_0_16,
              PPB_VideoDecoder_Dev)

#include "ppapi/thunk/interfaces_postamble.h"
