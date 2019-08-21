/*
 * Copyright (C) 2013 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef INCLUDED_BLPWTK2_CONFIG_H
#define INCLUDED_BLPWTK2_CONFIG_H

#include <windows.h>  // NOLINT

#if defined BUILDING_BLPWTK2_SHARED
#define BLPWTK2_EXPORT _declspec(dllexport)
#elif defined USING_BLPWTK2_SHARED
#define BLPWTK2_EXPORT _declspec(dllimport)
#else
#define BLPWTK2_EXPORT
#endif

#include <blpwtk2_version.h>

// The following is a list of macro definitions that enable specific features
// of blpwtk2.  The embedder can use these macros to determine if they can call
// a certain API.  The purpose behind the "feature #" comments is to help git
// resolve merge conflicts automatically by disambiguating the line additions
// for each feature.

// feature 0
#define BLPWTK2_FEATURE_FOCUS
// feature 1
// feature 2
#define BLPWTK2_FEATURE_PRINTPDF
// feature 3
#define BLPWTK2_FEATURE_CRTHANDLER
// feature 4
// feature 5
#define BLPWTK2_FEATURE_DEVTOOLSINTEGRATION
// feature 6
#define BLPWTK2_FEATURE_DUMPDIAGNOSTICS
// feature 7
#define BLPWTK2_FEATURE_PERFORMANCETIMING
// feature 8
// feature 9
// feature 10
// feature 11
#define BLPWTK2_FEATURE_DOCPRINTER
// feature 12
#define BLPWTK2_FEATURE_CUSTOMFONTS
// feature 13
// feature 14
// feature 15
// feature 16
// feature 17
// feature 18
// feature 19
// feature 20
// feature 21
// feature 22
#define BLPWTK2_FEATURE_LOGMESSAGEHANDLER
// feature 23
// feature 24
// feature 25
// feature 26
// feature 27
// feature 28
#define BLPWTK2_FEATURE_MULTIHEAPTRACER
// feature 29
// feature 30
// feature 31
// feature 32
#define BLPWTK2_FEATURE_BROWSER_V8
// feature 33
// feature 34
#define BLPWTK2_FEATURE_KEYBOARD_LAYOUT
// feature 35
// feature 36
// feature 37
// feature 38
// feature 39
#define BLPWTK2_FEATURE_EMBEDDER_IPC

namespace blpwtk2 {

// TODO: support other native handles
typedef HWND NativeView;
typedef HMONITOR NativeScreen;
typedef HFONT NativeFont;
typedef void* NativeViewForTransit;
typedef void* NativeScreenForTransit;
typedef MSG NativeMsg;
typedef HDC NativeDeviceContext;
typedef RECT NativeRect;
typedef COLORREF NativeColor;
typedef HRGN NativeRegion;

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_CONFIG_H

// vim: ts=4 et

