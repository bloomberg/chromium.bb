// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/thunk/interfaces_preamble.h"

// This file contains lists of interfaces. It's intended to be included by
// another file which defines implementations of the macros. This allows files
// to do specific registration tasks for each supported interface.


// Api categories
// --------------
// Enumerates the categories of APIs. These correspnd to the *_api.h files in
// this directory. One API may implement one or more actual interfaces.
//
// For PROXIED_APIs, these also correspond to *_Proxy objects. The proxied ones
// define factory functions for each of these classes. UNPROXIED_APIs are ones
// that exist in the webkit/plugins/ppapi/*_impl.h, but not in the proxy.
PROXIED_API(PPB_Audio)
PROXIED_API(PPB_Core)
PROXIED_API(PPB_FileRef)
PROXIED_API(PPB_FileSystem)
PROXIED_API(PPB_Graphics3D)
PROXIED_API(PPB_ImageData)
PROXIED_API(PPB_Instance)
PROXIED_API(PPB_URLLoader)

// AudioConfig isn't proxied in the normal way, we have only local classes and
// serialize it to a struct when we need it on the host side.
UNPROXIED_API(PPB_AudioConfig)

// Interfaces
// ----------
// Enumerates interfaces as (api_name, interface_name, interface_struct).
//
// The api_name corresponds to the class in the list above for the object
// that implements the API. Some things may be special and aren't implemented
// by any specific API object, and we use "NoAPIName" for those. Implementors
// of these macros should handle this case. There can be more than one line
// referring to the same api_name (typically different versions of the
// same interface).
//
// The interface_name is the string that corresponds to the interface.
//
// The interface_struct is the typename of the struct corresponding to the
// interface string.
// Note: Core is special and is registered manually.
PROXIED_IFACE(PPB_Audio, PPB_AUDIO_INTERFACE_1_0, PPB_Audio_1_0)
PROXIED_IFACE(PPB_FileRef, PPB_FILEREF_INTERFACE_1_0, PPB_FileRef_1_0)
PROXIED_IFACE(PPB_FileSystem, PPB_FILESYSTEM_INTERFACE_1_0, PPB_FileSystem_1_0)
PROXIED_IFACE(PPB_Graphics3D, PPB_GRAPHICS_3D_INTERFACE_1_0, PPB_Graphics3D_1_0)
PROXIED_IFACE(PPB_ImageData, PPB_IMAGEDATA_INTERFACE_1_0, PPB_ImageData_1_0)
PROXIED_IFACE(PPB_Instance, PPB_CONSOLE_INTERFACE_1_0, PPB_Console_1_0)
PROXIED_IFACE(PPB_Instance, PPB_GAMEPAD_INTERFACE_1_0, PPB_Gamepad_1_0)
PROXIED_IFACE(PPB_Instance, PPB_INSTANCE_INTERFACE_1_0, PPB_Instance_1_0)
PROXIED_IFACE(NoAPIName, PPB_FILEIO_INTERFACE_1_0, PPB_FileIO_1_0)
PROXIED_IFACE(NoAPIName, PPB_FILEIO_INTERFACE_1_1, PPB_FileIO_1_1)
PROXIED_IFACE(NoAPIName, PPB_GRAPHICS_2D_INTERFACE_1_0, PPB_Graphics2D_1_0)
PROXIED_IFACE(NoAPIName, PPB_INPUT_EVENT_INTERFACE_1_0, PPB_InputEvent_1_0)
PROXIED_IFACE(NoAPIName, PPB_KEYBOARD_INPUT_EVENT_INTERFACE_1_0,
              PPB_KeyboardInputEvent_1_0)
PROXIED_IFACE(NoAPIName, PPB_MOUSE_INPUT_EVENT_INTERFACE_1_0,
              PPB_MouseInputEvent_1_0)
PROXIED_IFACE(NoAPIName, PPB_MOUSE_INPUT_EVENT_INTERFACE_1_1,
              PPB_MouseInputEvent_1_1)
PROXIED_IFACE(NoAPIName, PPB_WHEEL_INPUT_EVENT_INTERFACE_1_0,
              PPB_WheelInputEvent_1_0)
PROXIED_IFACE(NoAPIName, PPB_TOUCH_INPUT_EVENT_INTERFACE_1_0,
              PPB_TouchInputEvent_1_0)
PROXIED_IFACE(PPB_Instance, PPB_FULLSCREEN_INTERFACE_1_0, PPB_Fullscreen_1_0)
PROXIED_IFACE(PPB_Instance, PPB_MESSAGING_INTERFACE_1_0, PPB_Messaging_1_0)
PROXIED_IFACE(PPB_Instance, PPB_MOUSECURSOR_INTERFACE_1_0, PPB_MouseCursor_1_0)
PROXIED_IFACE(PPB_Instance, PPB_MOUSELOCK_INTERFACE_1_0, PPB_MouseLock_1_0)
PROXIED_IFACE(PPB_URLLoader, PPB_URLLOADER_INTERFACE_1_0, PPB_URLLoader_1_0)
PROXIED_IFACE(NoAPIName, PPB_URLREQUESTINFO_INTERFACE_1_0,
              PPB_URLRequestInfo_1_0)
PROXIED_IFACE(NoAPIName, PPB_URLRESPONSEINFO_INTERFACE_1_0,
              PPB_URLResponseInfo_1_0)
PROXIED_IFACE(NoAPIName, PPB_WEBSOCKET_INTERFACE_1_0, PPB_WebSocket_1_0)

// Note: PPB_Var and PPB_VarArrayBuffer are special and registered manually.
PROXIED_IFACE(NoAPIName, PPB_VIEW_INTERFACE_1_0, PPB_View_1_0)

// This has no corresponding _Proxy object since it does no IPC.
PROXIED_IFACE(NoAPIName, PPB_AUDIO_CONFIG_INTERFACE_1_0, PPB_AudioConfig_1_0)
PROXIED_IFACE(NoAPIName, PPB_AUDIO_CONFIG_INTERFACE_1_1, PPB_AudioConfig_1_1)

#include "ppapi/thunk/interfaces_postamble.h"
