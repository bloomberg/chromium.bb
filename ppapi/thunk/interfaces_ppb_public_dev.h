// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Please see inteface_ppb_public_stable for the documentation on the format of
// this file.

#include "ppapi/thunk/interfaces_preamble.h"

PROXIED_API(PPB_AudioInput)
PROXIED_API(PPB_Buffer)
PROXIED_API(PPB_CursorControl)
UNPROXIED_API(PPB_DirectoryReader)
PROXIED_API(PPB_FileChooser)
PROXIED_API(PPB_Font)
PROXIED_API(PPB_Graphics3D)
UNPROXIED_API(PPB_LayerCompositor)
UNPROXIED_API(PPB_Scrollbar)
PROXIED_API(PPB_TextInput)
UNPROXIED_API(PPB_Transport)
PROXIED_API(PPB_VideoCapture)
PROXIED_API(PPB_VideoDecoder)
UNPROXIED_API(PPB_WebSocket)
UNPROXIED_API(PPB_Widget)

PROXIED_IFACE(PPB_AudioInput, PPB_AUDIO_INPUT_DEV_INTERFACE_0_1,
              PPB_AudioInput_Dev)
PROXIED_IFACE(NoAPIName, PPB_IME_INPUT_EVENT_DEV_INTERFACE_0_1,
              PPB_IMEInputEvent_Dev)
PROXIED_IFACE(PPB_Buffer, PPB_BUFFER_DEV_INTERFACE_0_4, PPB_Buffer_Dev)
PROXIED_IFACE(PPB_Graphics3D, PPB_GLES_CHROMIUM_TEXTURE_MAPPING_DEV_INTERFACE,
              PPB_GLESChromiumTextureMapping_Dev)
PROXIED_IFACE(NoAPIName, PPB_CRYPTO_DEV_INTERFACE, PPB_Crypto_Dev)
PROXIED_IFACE(PPB_CursorControl, PPB_CURSOR_CONTROL_DEV_INTERFACE_0_4,
              PPB_CursorControl_Dev)
UNPROXIED_IFACE(PPB_DirectoryReader, PPB_DIRECTORYREADER_DEV_INTERFACE_0_5,
                PPB_DirectoryReader_Dev)
UNPROXIED_IFACE(PPB_Find, PPB_FIND_DEV_INTERFACE_0_3, PPB_Find_Dev)
PROXIED_IFACE(PPB_FileChooser, PPB_FILECHOOSER_DEV_INTERFACE_0_5,
              PPB_FileChooser_Dev)
PROXIED_IFACE(PPB_Font, PPB_FONT_DEV_INTERFACE_0_6, PPB_Font_Dev)
PROXIED_IFACE(PPB_Instance, PPB_CHAR_SET_DEV_INTERFACE_0_4, PPB_CharSet_Dev)
PROXIED_IFACE(PPB_Instance, PPB_CONSOLE_DEV_INTERFACE, PPB_Console_Dev)
PROXIED_IFACE(PPB_Instance, PPB_URLUTIL_DEV_INTERFACE_0_6, PPB_URLUtil_Dev)
UNPROXIED_IFACE(PPB_Instance, PPB_ZOOM_DEV_INTERFACE_0_2, PPB_Zoom_Dev)
UNPROXIED_IFACE(PPB_LayerCompositor, PPB_LAYER_COMPOSITOR_DEV_INTERFACE_0_2,
                PPB_LayerCompositor_Dev)
PROXIED_IFACE(NoAPIName, PPB_MEMORY_DEV_INTERFACE, PPB_Memory_Dev)
UNPROXIED_IFACE(PPB_Scrollbar, PPB_SCROLLBAR_DEV_INTERFACE_0_5,
                PPB_Scrollbar_Dev)
PROXIED_IFACE(PPB_TextInput, PPB_TEXTINPUT_DEV_INTERFACE_0_1,
              PPB_TextInput_Dev)
UNPROXIED_IFACE(PPB_Transport, PPB_TRANSPORT_DEV_INTERFACE_0_7,
                PPB_Transport_Dev)
PROXIED_IFACE(PPB_VideoCapture, PPB_VIDEOCAPTURE_DEV_INTERFACE_0_1,
              PPB_VideoCapture_Dev)
PROXIED_IFACE(PPB_VideoDecoder, PPB_VIDEODECODER_DEV_INTERFACE_0_16,
              PPB_VideoDecoder_Dev)
UNPROXIED_IFACE(PPB_VideoLayer, PPB_VIDEOLAYER_DEV_INTERFACE,
                PPB_VideoLayer_Dev)
UNPROXIED_IFACE(PPB_WebSocket, PPB_WEBSOCKET_DEV_INTERFACE_0_1,
                PPB_WebSocket_Dev)
UNPROXIED_IFACE(PPB_Widget, PPB_WIDGET_DEV_INTERFACE_0_3, PPB_Widget_Dev)

#include "ppapi/thunk/interfaces_postamble.h"
