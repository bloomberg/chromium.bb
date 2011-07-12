// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Defines helper functions for all interfaces supported by the Native Client
// proxy.

#ifndef NATIVE_CLIENT_TESTS_PPAPI_TEST_PPB_TEMPLATE_GET_BROWSER_INTERFACE_H
#define NATIVE_CLIENT_TESTS_PPAPI_TEST_PPB_TEMPLATE_GET_BROWSER_INTERFACE_H

struct PPB_Context3D_Dev;
struct PPB_Core;
struct PPB_FileIO_Dev;
struct PPB_FileSystem_Dev;
struct PPB_Graphics2D;
struct PPB_ImageData;
struct PPB_Instance;
struct PPB_Messaging;
struct PPB_Scrollbar_Dev;
struct PPB_Surface3D_Dev;
struct PPB_Testing_Dev;
struct PPB_URLLoader;
struct PPB_URLRequestInfo;
struct PPB_URLResponseInfo;
struct PPB_Var;

// Looks up the interface and returns its pointer or NULL.
const void* GetBrowserInterface(const char* interface_name);
// Uses GetBrowserInterface() and CHECKs for NULL.
const void* GetBrowserInterfaceSafe(const char* interface_name);

//
// Stable interfaces.
// Lookup guarantees that the interface is available by using NULL CHECKs.
//

const PPB_Core* PPBCore();
const PPB_Graphics2D* PPBGraphics2D();
const PPB_ImageData* PPBImageData();
const PPB_Instance* PPBInstance();
const PPB_Messaging* PPBMessaging();
const PPB_URLLoader* PPBURLLoader();
const PPB_URLRequestInfo* PPBURLRequestInfo();
const PPB_URLResponseInfo* PPBURLResponseInfo();
const PPB_Var* PPBVar();

//
// Experimental (aka Dev) interfaces.
// Lookup returns NULL if the interface is not available.
//

const PPB_Context3D_Dev* PPBContext3DDev();
const PPB_FileIO_Dev* PPBFileIODev();
const PPB_FileSystem_Dev* PPBFileSystemDev();
const PPB_Scrollbar_Dev* PPBScrollbarDev();
const PPB_Surface3D_Dev* PPBSurface3DDev();
const PPB_Testing_Dev* PPBTestingDev();

#endif  // NATIVE_CLIENT_TESTS_PPAPI_TEST_PPB_TEMPLATE_GET_BROWSER_INTERFACE_H
