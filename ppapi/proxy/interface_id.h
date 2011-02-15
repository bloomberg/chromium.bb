// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_INTERFACE_ID_H_
#define PPAPI_PROXY_INTERFACE_ID_H_

namespace pp {
namespace proxy {

// These numbers must be all small integers. They are used in a lookup table
// to route messages to the appropriate message handler.
enum InterfaceID {
  // Zero is reserved for control messages.
  INTERFACE_ID_NONE = 0,
  INTERFACE_ID_PPB_AUDIO = 1,
  INTERFACE_ID_PPB_AUDIO_CONFIG,
  INTERFACE_ID_PPB_BUFFER,
  INTERFACE_ID_PPB_CHAR_SET,
  INTERFACE_ID_PPB_CONTEXT_3D,
  INTERFACE_ID_PPB_CORE,
  INTERFACE_ID_PPB_CURSORCONTROL,
  INTERFACE_ID_PPB_FLASH,
  INTERFACE_ID_PPB_FLASH_MENU,
  INTERFACE_ID_PPB_FONT,
  INTERFACE_ID_PPB_FULLSCREEN,
  INTERFACE_ID_PPB_GLES_CHROMIUM_TM,
  INTERFACE_ID_PPB_GRAPHICS_2D,
  INTERFACE_ID_PPB_IMAGE_DATA,
  INTERFACE_ID_PPB_INSTANCE,
  INTERFACE_ID_PPB_OPENGLES2,
  INTERFACE_ID_PPB_PDF,
  INTERFACE_ID_PPB_SURFACE_3D,
  INTERFACE_ID_PPB_TESTING,
  INTERFACE_ID_PPB_URL_LOADER,
  INTERFACE_ID_PPB_URL_LOADER_TRUSTED,
  INTERFACE_ID_PPB_URL_REQUEST_INFO,
  INTERFACE_ID_PPB_URL_RESPONSE_INFO,
  INTERFACE_ID_PPB_VAR,
  INTERFACE_ID_PPB_VAR_DEPRECATED,

  INTERFACE_ID_PPP_CLASS,
  INTERFACE_ID_PPP_INSTANCE,

  // Must be last to indicate the number of interface IDs.
  INTERFACE_ID_COUNT
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_INTERFACE_ID_H_
