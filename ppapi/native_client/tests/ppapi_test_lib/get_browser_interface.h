// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Defines helper functions for all interfaces supported by the Native Client
// proxy.

#ifndef NATIVE_CLIENT_TESTS_PPAPI_TEST_PPB_TEMPLATE_GET_BROWSER_INTERFACE_H
#define NATIVE_CLIENT_TESTS_PPAPI_TEST_PPB_TEMPLATE_GET_BROWSER_INTERFACE_H

// The definition of scrollbar depends on the interface version.
// TODO(brettw) either move all interfaces to this method, or encode all
// versions explicitly in all interfaces.
#include "ppapi/c/dev/ppb_scrollbar_dev.h"

struct PPB_Core;
struct PPB_CursorControl_Dev;
struct PPB_FileIO;
struct PPB_FileRef;
struct PPB_FileSystem;
struct PPB_Font_Dev;
struct PPB_Fullscreen;
struct PPB_Graphics2D;
struct PPB_Graphics3D;
struct PPB_ImageData;
struct PPB_InputEvent;
struct PPB_Instance;
struct PPB_KeyboardInputEvent;
struct PPB_Memory_Dev;
struct PPB_Messaging;
struct PPB_MouseInputEvent;
struct PPB_OpenGLES2;
struct PPB_Testing_Dev;
struct PPB_URLLoader;
struct PPB_URLRequestInfo;
struct PPB_URLResponseInfo;
struct PPB_Var;
struct PPB_WheelInputEvent;
struct PPB_Widget_Dev;

// Looks up the interface and returns its pointer or NULL.
const void* GetBrowserInterface(const char* interface_name);
// Uses GetBrowserInterface() and CHECKs for NULL.
const void* GetBrowserInterfaceSafe(const char* interface_name);

//
// Stable interfaces.
// Lookup guarantees that the interface is available by using NULL CHECKs.
//

const PPB_Core* PPBCore();
const PPB_FileIO* PPBFileIO();
const PPB_FileRef* PPBFileRef();
const PPB_FileSystem* PPBFileSystem();
const PPB_Fullscreen* PPBFullscreen();
const PPB_Graphics2D* PPBGraphics2D();
const PPB_Graphics3D* PPBGraphics3D();
const PPB_ImageData* PPBImageData();
const PPB_InputEvent* PPBInputEvent();
const PPB_Instance* PPBInstance();
const PPB_KeyboardInputEvent* PPBKeyboardInputEvent();
const PPB_Messaging* PPBMessaging();
const PPB_MouseInputEvent* PPBMouseInputEvent();
const PPB_OpenGLES2* PPBOpenGLES2();
const PPB_URLLoader* PPBURLLoader();
const PPB_URLRequestInfo* PPBURLRequestInfo();
const PPB_URLResponseInfo* PPBURLResponseInfo();
const PPB_Var* PPBVar();
const PPB_WheelInputEvent* PPBWheelInputEvent();

//
// Experimental (aka Dev) interfaces.
// Lookup returns NULL if the interface is not available.
//

const PPB_CursorControl_Dev* PPBCursorControlDev();
const PPB_Font_Dev* PPBFontDev();
const PPB_Memory_Dev* PPBMemoryDev();
const PPB_Scrollbar_Dev* PPBScrollbarDev();
const PPB_Testing_Dev* PPBTestingDev();
const PPB_Widget_Dev* PPBWidgetDev();

#endif  // NATIVE_CLIENT_TESTS_PPAPI_TEST_PPB_TEMPLATE_GET_BROWSER_INTERFACE_H
